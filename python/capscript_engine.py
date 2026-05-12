import os
import sys
import re
import json
import time
import random
import base64
import threading
import urllib.request
import urllib.error
import urllib.parse
import configparser
import html
import xml.etree.ElementTree as ET
from datetime import datetime
from concurrent.futures import ThreadPoolExecutor, as_completed

import yt_dlp
from cryptography.fernet import Fernet
from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.primitives.kdf.pbkdf2 import PBKDF2HMAC

MAX_WORKERS = 3
_YTDLP_SOCKET_TIMEOUT_SEC = 8
_YTDLP_RETRY_COUNT = 0
_REQUEST_JITTER_MIN_SEC = 0.35
_REQUEST_JITTER_MAX_SEC = 1.25
_TRANSCRIPT_CACHE_DIRNAME = "transcript_cache"
_TRANSCRIPT_CACHE_SCHEMA = 1

_SUBTITLE_SEMAPHORE = threading.Semaphore(2)
_VALID_MATCH_MODES = {
    "smart",
    "exact_phrase",
    "contains",
}
_SEARCH_TOKEN_RX = re.compile(r"\w+(?:[’'-]\w+)*", flags=re.UNICODE)
_SPECIAL_QUERY_CHAR_RX = re.compile(r"[^\w\s'\"’-]")
_WEBSHARE_PROXY_HOSTS = (
    "p.webshare.io:80",
    "proxy.webshare.io:80",
)

def _cooperative_sleep(seconds: float, cancel_check=None):
    deadline = time.monotonic() + max(0.0, float(seconds))
    while True:
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            return
        if cancel_check:
            cancel_check()
        time.sleep(min(0.25, remaining))

def _jittered_request_pause(cancel_check=None, minimum=None, maximum=None):
    low = _REQUEST_JITTER_MIN_SEC if minimum is None else max(0.0, float(minimum))
    high = _REQUEST_JITTER_MAX_SEC if maximum is None else max(low, float(maximum))
    _cooperative_sleep(random.uniform(low, high), cancel_check=cancel_check)

def get_yt_dlp_version() -> str:
    try:
        return yt_dlp.version.__version__
    except Exception:
        return "unknown"

def _get_application_root_path():
    if getattr(sys, "frozen", False):
        return os.path.dirname(sys.executable)
    return os.path.dirname(os.path.abspath(__file__))

PREFERENCES_FILE_PATH = os.path.join(_get_application_root_path(), "preferences.ini")
_PREFERENCES_SECTION  = "Preferences"
_API_KEY_OPTION        = "API_KEY"
_PROXY_SECTION         = "Proxy"
_PROXY_TYPE_OPTION     = "type"
_PROXY_USERNAME_OPTION = "username"
_PROXY_PASSWORD_OPTION = "password"
_PROXY_URL_OPTION      = "url"

def _build_ydl_opts(
    language=None,
    cookies_file=None,
    cookies_from_browser=None,
    proxy_type=None,
    proxy_username=None,
    proxy_password=None,
    proxy_url=None,
    proxy_candidates=None,
    extra_opts=None,
):
    opts = {
        "quiet":         True,
        "no_warnings":   True,
        "skip_download": True,
        "ignoreerrors":  True,
        "ignoreconfig":  True,
        "socket_timeout": _YTDLP_SOCKET_TIMEOUT_SEC,
        "retries": _YTDLP_RETRY_COUNT,
        "extractor_retries": _YTDLP_RETRY_COUNT,
        "fragment_retries": _YTDLP_RETRY_COUNT,
        "file_access_retries": _YTDLP_RETRY_COUNT,
    }

    candidates = list(
        proxy_candidates
        or _build_proxy_candidates(
            proxy_type,
            proxy_username,
            proxy_password,
            proxy_url,
        )
    )
    if candidates:
        opts["proxy"] = candidates[0]

    if not cookies_file:
        default = os.path.join(_get_application_root_path(), "cookies.txt")
        if os.path.exists(default):
            cookies_file = default
    if cookies_file and os.path.exists(cookies_file):
        opts["cookiefile"] = cookies_file
    elif cookies_from_browser:
        browser = str(cookies_from_browser).strip().lower()
        if browser:
            if browser == "vivaldi" and os.name == "nt":

                local_appdata = os.environ.get("LOCALAPPDATA", "")
                vivaldi_profile = os.path.join(
                    local_appdata,
                    "Vivaldi",
                    "User Data",
                    "Default",
                )
                opts["cookiesfrombrowser"] = ("vivaldi", vivaldi_profile)
            else:
                opts["cookiesfrombrowser"] = (browser,)

    if cookies_file or cookies_from_browser:
        opts["remote_components"] = ["ejs:github"]

    if language:
        opts["subtitleslangs"]   = [language, f"{language}-orig", f"{language}-*", ".*"]
        opts["writesubtitles"]   = True
        opts["writeautomaticsub"] = True
        opts["subtitlesformat"]  = "json3"

    if extra_opts:
        opts.update(extra_opts)

    return opts

def _sanitize_error_message(message: str) -> str:
    if not message:
        return message
    return re.sub(r"://([^:@/\s]+):([^@/\s]+)@", r"://\1:***@", message)

def _augment_error_with_restart_hint(message: str) -> str:
    """
    If the error looks like the Chrome cookie DB copy problem from yt-dlp,
    append a short user-facing hint suggesting a browser restart and retry.
    """
    if not message:
        return message
    lower = message.lower()
    if "could not copy chrome cookie" in lower or "could not copy chrome cookie database" in lower:
        hint = (
            "\n[HINT]  Detected Chrome cookie DB access error — "
            "try restarting your browser and press the restart button to retry."
        )

        if hint.strip() not in message:
            return message + hint
    return message

def _normalize_proxy_url(proxy_url: str | None):
    raw = (proxy_url or "").strip()
    if not raw:
        return None

    if "://" not in raw:
        raw = f"http://{raw}"

    parsed = urllib.parse.urlsplit(raw)
    scheme = (parsed.scheme or "").lower() or "http"
    if scheme == "socks5":

        scheme = "socks5h"

    allowed = {"http", "https", "socks5", "socks5h", "socks4", "socks4a"}
    if scheme not in allowed:
        return raw

    host = parsed.hostname
    if not host:
        return raw

    username = parsed.username
    password = parsed.password
    auth = ""
    if username is not None:
        auth = urllib.parse.quote(username, safe="")
        if password is not None:
            auth += ":" + urllib.parse.quote(password, safe="")
        auth += "@"

    host_part = host
    if ":" in host and not host.startswith("["):
        host_part = f"[{host}]"

    netloc = auth + host_part
    if parsed.port:
        netloc += f":{parsed.port}"

    return urllib.parse.urlunsplit(
        (scheme, netloc, parsed.path or "", parsed.query or "", parsed.fragment or "")
    )

def _webshare_hostport_from_proxy_url(proxy_url: str | None):
    normalized = _normalize_proxy_url(proxy_url)
    if not normalized:
        return None

    parsed = urllib.parse.urlsplit(normalized)
    host = parsed.hostname
    if not host:
        return None

    host_part = host
    if ":" in host and not host.startswith("["):
        host_part = f"[{host}]"
    port = parsed.port or 80
    return f"{host_part}:{port}"

def _proxy_url_has_auth(proxy_url: str | None) -> bool:
    normalized = _normalize_proxy_url(proxy_url)
    if not normalized:
        return False
    try:
        parsed = urllib.parse.urlsplit(normalized)
        return parsed.username is not None and parsed.password is not None
    except Exception:
        return False

def _describe_proxy_candidate(proxy_value: str | None) -> str:
    if not proxy_value:
        return "direct"
    try:
        parsed = urllib.parse.urlsplit(proxy_value)
        host = parsed.hostname or "?"
        port = parsed.port
        return f"{host}:{port}" if port else host
    except Exception:
        return "proxy"

def _ydl_uses_cookie_auth(ydl_opts: dict) -> bool:
    return bool(ydl_opts.get("cookiefile") or ydl_opts.get("cookiesfrombrowser"))

def _build_proxy_candidates(
    proxy_type=None,
    proxy_username=None,
    proxy_password=None,
    proxy_url=None,
):
    p_type = (proxy_type or "").strip().lower()
    if p_type == "none":
        p_type = ""

    if p_type == "webshare":

        normalized_direct = _normalize_proxy_url(proxy_url)
        if normalized_direct and _proxy_url_has_auth(normalized_direct):
            return [normalized_direct]

        user = urllib.parse.unquote((proxy_username or "").strip())
        pwd = urllib.parse.unquote((proxy_password or "").strip())
        if not user or not pwd:
            return []

        q_user = urllib.parse.quote(user, safe="")
        q_pwd = urllib.parse.quote(pwd, safe="")

        endpoints = []
        custom_hostport = _webshare_hostport_from_proxy_url(proxy_url)
        if custom_hostport:
            endpoints.append(custom_hostport)
        else:
            endpoints.extend(_WEBSHARE_PROXY_HOSTS)

        seen = set()
        deduped_endpoints = []
        for endpoint in endpoints:
            if endpoint in seen:
                continue
            seen.add(endpoint)
            deduped_endpoints.append(endpoint)

        return [f"http://{q_user}:{q_pwd}@{host}" for host in deduped_endpoints]

    if p_type == "generic":
        normalized = _normalize_proxy_url(proxy_url)
        return [normalized] if normalized else []

    return []

def _extract_info_with_proxy_fallback(
    url: str,
    ydl_opts: dict,
    proxy_candidates=None,
    cancel_check=None,
):
    candidates = list(proxy_candidates or [])
    primary = ydl_opts.get("proxy")
    if primary and primary not in candidates:
        candidates.insert(0, primary)
    if _ydl_uses_cookie_auth(ydl_opts) and None not in candidates:
        candidates.append(None)
    if not candidates:
        candidates = [None]

    last_error = None
    for candidate in candidates:
        if cancel_check:
            cancel_check()

        _jittered_request_pause(cancel_check=cancel_check, minimum=0.2, maximum=0.9)

        opts = dict(ydl_opts)
        if candidate:
            opts["proxy"] = candidate
        else:
            opts.pop("proxy", None)

        try:
            with yt_dlp.YoutubeDL(opts) as ydl:
                info = ydl.extract_info(url, download=False)
            if info:
                return info
            last_error = RuntimeError("yt-dlp returned no info")
        except Exception as exc:
            last_error = exc

    raise RuntimeError(_sanitize_error_message(str(last_error or "yt-dlp extraction failed")))

def _find_best_lang_key(subs: dict, language: str):
    """
    Return the best matching key from a subtitle dict for the requested language.
    Priority: exact → '-orig' suffix → any prefix match.
    """
    if not subs:
        return None, None
    if language in subs:
        return language, "exact"
    orig_key = f"{language}-orig"
    if orig_key in subs:
        return orig_key, "orig"
    for k in subs:
        if k.startswith(language):
            return k, "prefix"
    return None, None

def _fetch_transcript_json3(
    info: dict,
    language: str,
    ydl_instance=None,
    log_fn=None,
    cancel_check=None,
):
    """
    Find the best subtitle track for `language`, fetch its json3 URL, and return
    a list of {'start': float, 'dur': float, 'text': str}, or None on failure.

    Uses ydl_instance.urlopen() when available so the request is routed through
    whatever proxy is configured in the YoutubeDL opts — this is the critical fix
    for 429 errors that occurred when urllib.request bypassed the proxy entirely.

    Manual subtitles are preferred over automatic captions.
    Retries up to 3 times with exponential back-off on HTTP 429.
    """
    def _log(msg):
        if log_fn:
            try:
                log_fn(msg)
            except Exception:
                pass

    subs_manual = info.get("subtitles") or {}
    subs_auto   = info.get("automatic_captions") or {}

    selected_track = None

    for sub_dict, kind in [(subs_manual, "manual"), (subs_auto, "auto")]:
        lang_key, match_type = _find_best_lang_key(sub_dict, language)
        if not lang_key:
            continue
        formats = sub_dict.get(lang_key) or []
        if not formats:
            continue
        preferred_formats = []
        for wanted in ("json3", "vtt", "srv3", "srv2", "srv1", "ttml", "xml", "sbv"):
            for fmt in formats:
                if (fmt.get("ext") or "").strip().lower() == wanted and fmt not in preferred_formats:
                    preferred_formats.append(fmt)
        for fmt in formats:
            if fmt not in preferred_formats:
                preferred_formats.append(fmt)
        for fmt in preferred_formats:
            url = fmt.get("url")
            if url:
                selected_track = {
                    "url": url,
                    "ext": (fmt.get("ext") or "").strip().lower(),
                    "desc": f"{lang_key} {kind}",
                }
                break
        if selected_track:
            break

    if not selected_track:
        _log(f"[SUBS]  No {language} track available")
        return None

    _log(f"[SUBS]  {selected_track['desc']} — fetching")

    for attempt in range(3):
        if cancel_check:
            cancel_check()

        _jittered_request_pause(cancel_check=cancel_check, minimum=0.2, maximum=0.8)

        if attempt > 0:

            wait = (2 ** attempt) + random.uniform(0.5, 2.5)
            _log(f"[WARN]  429 rate-limited — retry {attempt}/2 in {wait:.1f}s")
            _cooperative_sleep(wait, cancel_check=cancel_check)

        try:
            if ydl_instance is not None:

                resp = ydl_instance.urlopen(selected_track["url"])
                payload_text = resp.read().decode("utf-8", errors="replace")
            else:

                req = urllib.request.Request(
                    selected_track["url"], headers={"User-Agent": "Mozilla/5.0"}
                )
                with urllib.request.urlopen(req, timeout=_YTDLP_SOCKET_TIMEOUT_SEC) as r:
                    payload_text = r.read().decode("utf-8", errors="replace")

            transcript = _parse_subtitle_payload(payload_text, selected_track["ext"])
            if transcript:
                _log(f"[SUBS]  {len(transcript):,} segments parsed")
                return transcript
            _log(f"[WARN]  Track returned 0 segments ({selected_track['ext'] or 'unknown'})")
            return None

        except urllib.error.HTTPError as exc:
            if exc.code == 429 and attempt < 2:
                continue          
            _log(f"[ERR]   Subtitle fetch failed: HTTP {exc.code}")
            return None
        except Exception as exc:

            msg = str(exc)
            if "429" in msg and attempt < 2:
                continue
            _log(f"[ERR]   Subtitle fetch failed: {exc}")
            return None

    _log("[ERR]   Subtitle fetch failed: 429 after 3 attempts")
    return None

def _parse_json3(data):
    result = []
    for event in data.get("events", []):
        start_ms = event.get("tStartMs", 0)
        dur_ms   = event.get("dDurationMs", 0)
        segs     = event.get("segs", [])
        text     = "".join(s.get("utf8", "") for s in segs).strip()
        text     = re.sub(r"<[^>]+>", "", text).strip()
        if text and text != "\n":
            result.append(
                {"start": start_ms / 1000.0, "dur": dur_ms / 1000.0, "text": text}
            )
    return result

def _parse_subtitle_timestamp(timestamp):
    text = (timestamp or "").strip().replace(",", ".")
    if not text:
        return None
    try:
        parts = text.split(":")
        if len(parts) == 3:
            hours, minutes, seconds = parts
            return int(hours) * 3600 + int(minutes) * 60 + float(seconds)
        if len(parts) == 2:
            minutes, seconds = parts
            return int(minutes) * 60 + float(seconds)
        return float(text)
    except Exception:
        return None

def _parse_vtt(text):
    result = []
    lines = [line.rstrip("\n") for line in (text or "").splitlines()]
    idx = 0
    while idx < len(lines):
        line = lines[idx].strip()
        if not line or line == "WEBVTT" or line.startswith("NOTE"):
            idx += 1
            continue
        if "-->" not in line:
            idx += 1
            continue

        start_text, end_text = [part.strip() for part in line.split("-->", 1)]
        start = _parse_subtitle_timestamp(start_text)
        end = _parse_subtitle_timestamp(end_text.split()[0])
        idx += 1

        cue_lines = []
        while idx < len(lines):
            cue_line = lines[idx].rstrip()
            if not cue_line.strip():
                break
            cue_lines.append(cue_line)
            idx += 1

        cue_text = html.unescape(re.sub(r"<[^>]+>", "", " ".join(cue_lines))).strip()
        if cue_text and start is not None:
            duration = max(0.0, (end - start)) if end is not None else 0.0
            result.append({"start": start, "dur": duration, "text": cue_text})

        idx += 1
    return result

def _parse_subtitle_xml(text):
    result = []
    try:
        root = ET.fromstring(text)
    except Exception:
        return result

    for element in root.iter():
        if not element.tag.lower().endswith("text"):
            continue
        start_raw = element.attrib.get("start") or element.attrib.get("begin")
        if not start_raw:
            continue
        start = _parse_subtitle_timestamp(start_raw)
        if start is None:
            continue

        duration_raw = element.attrib.get("dur")
        end_raw = element.attrib.get("end")
        duration = _parse_subtitle_timestamp(duration_raw) if duration_raw else None
        if duration is None and end_raw:
            end_value = _parse_subtitle_timestamp(end_raw)
            if end_value is not None:
                duration = max(0.0, end_value - start)

        cue_text = html.unescape("".join(element.itertext())).strip()
        cue_text = re.sub(r"<[^>]+>", "", cue_text).strip()
        if cue_text:
            result.append({"start": start, "dur": duration or 0.0, "text": cue_text})

    return result

def _parse_subtitle_payload(text, ext=None):
    format_hint = (ext or "").strip().lower()
    payload = text or ""

    if format_hint == "json3":
        try:
            parsed = _parse_json3(json.loads(payload))
            if parsed:
                return parsed
        except Exception:
            pass

    if format_hint in {"vtt", "webvtt"}:
        parsed = _parse_vtt(payload)
        if parsed:
            return parsed

    if format_hint in {"ttml", "xml", "srv1", "srv2", "srv3", "sbv"}:
        parsed = _parse_subtitle_xml(payload)
        if parsed:
            return parsed

    stripped = payload.lstrip()
    if stripped.startswith("{") or stripped.startswith("["):
        try:
            parsed = _parse_json3(json.loads(payload))
            if parsed:
                return parsed
        except Exception:
            pass

    if "-->" in payload:
        parsed = _parse_vtt(payload)
        if parsed:
            return parsed

    return _parse_subtitle_xml(payload)

def _get_encryption_key():
    machine_id = os.environ.get("COMPUTERNAME", "default") + os.environ.get(
        "USERNAME", "user"
    )
    salt = machine_id.encode()[:16].ljust(16, b"0")
    kdf = PBKDF2HMAC(
        algorithm=hashes.SHA256(), length=32, salt=salt, iterations=100000
    )
    return base64.urlsafe_b64encode(kdf.derive(b"CapScriptProAPIKey2025"))

def _encrypt_api_key(api_key):
    try:
        if not api_key:
            return ""
        fernet = Fernet(_get_encryption_key())
        return base64.urlsafe_b64encode(fernet.encrypt(api_key.encode())).decode()
    except Exception:
        return api_key

def _decrypt_api_key(encrypted_key):
    try:
        if not encrypted_key:
            return ""
        fernet = Fernet(_get_encryption_key())
        return fernet.decrypt(
            base64.urlsafe_b64decode(encrypted_key.encode())
        ).decode()
    except Exception:
        return encrypted_key

def save_proxy_settings(
    proxy_type: str,
    proxy_username: str = "",
    proxy_password: str = "",
    proxy_url: str = "",
) -> bool:
    config = configparser.ConfigParser()
    if os.path.exists(PREFERENCES_FILE_PATH):
        try:
            config.read(PREFERENCES_FILE_PATH, encoding="utf-8")
        except Exception:
            pass
    config[_PROXY_SECTION] = {
        _PROXY_TYPE_OPTION:     proxy_type or "none",
        _PROXY_USERNAME_OPTION: proxy_username or "",
        _PROXY_PASSWORD_OPTION: (
            _encrypt_api_key(proxy_password) if proxy_password else ""
        ),
        _PROXY_URL_OPTION: proxy_url or "",
    }
    try:
        directory = os.path.dirname(PREFERENCES_FILE_PATH)
        if directory:
            os.makedirs(directory, exist_ok=True)
        with open(PREFERENCES_FILE_PATH, "w", encoding="utf-8") as f:
            config.write(f)
        return True
    except IOError:
        return False

def load_proxy_settings() -> str:
    config   = configparser.ConfigParser()
    defaults = {"type": "none", "username": "", "password": "", "url": ""}
    if not os.path.exists(PREFERENCES_FILE_PATH):
        return json.dumps(defaults)
    try:
        config.read(PREFERENCES_FILE_PATH, encoding="utf-8")
        proxy_type     = config.get(_PROXY_SECTION, _PROXY_TYPE_OPTION,     fallback="none")
        proxy_username = config.get(_PROXY_SECTION, _PROXY_USERNAME_OPTION, fallback="")
        enc_password   = config.get(_PROXY_SECTION, _PROXY_PASSWORD_OPTION, fallback="")
        proxy_password = _decrypt_api_key(enc_password) if enc_password else ""
        proxy_url      = config.get(_PROXY_SECTION, _PROXY_URL_OPTION,      fallback="")
        return json.dumps({
            "type":     proxy_type,
            "username": proxy_username,
            "password": proxy_password,
            "url":      proxy_url,
        })
    except Exception:
        return json.dumps(defaults)

def validate_api_key(api_key: str) -> bool:
    return True

def save_api_key(api_key: str) -> bool:
    config    = configparser.ConfigParser()
    directory = os.path.dirname(PREFERENCES_FILE_PATH)
    if directory:
        os.makedirs(directory, exist_ok=True)
    if os.path.exists(PREFERENCES_FILE_PATH):
        try:
            config.read(PREFERENCES_FILE_PATH, encoding="utf-8")
        except Exception:
            pass
    if _PREFERENCES_SECTION not in config:
        config[_PREFERENCES_SECTION] = {}
    config[_PREFERENCES_SECTION][_API_KEY_OPTION] = _encrypt_api_key(api_key)
    try:
        with open(PREFERENCES_FILE_PATH, "w", encoding="utf-8") as f:
            config.write(f)
        return True
    except IOError:
        return False

def load_api_key() -> str:
    config = configparser.ConfigParser()
    if not os.path.exists(PREFERENCES_FILE_PATH):
        return ""
    try:
        config.read(PREFERENCES_FILE_PATH, encoding="utf-8")
        encrypted = config.get(_PREFERENCES_SECTION, _API_KEY_OPTION, fallback="")
        return _decrypt_api_key(encrypted)
    except Exception:
        return ""

def resolve_channel_id(
    api_key: str,
    channel_input: str,
    cookies_file=None,
    cookies_from_browser=None,
    proxy_type=None,
    proxy_username=None,
    proxy_password=None,
    proxy_url=None,
    cancel_check=None,
) -> str:
    channel_input = channel_input.strip()
    if not channel_input:
        return ""
    if channel_input.startswith("UC") and len(channel_input) >= 24:
        return channel_input

    if channel_input.startswith("http"):
        url = channel_input
    elif channel_input.startswith("@"):
        url = f"https://www.youtube.com/{channel_input}"
    else:
        url = f"https://www.youtube.com/@{channel_input}"

    proxy_candidates = _build_proxy_candidates(
        proxy_type,
        proxy_username,
        proxy_password,
        proxy_url,
    )
    if (proxy_type or "").strip().lower() == "webshare" and not proxy_candidates:
        print("[CapScript] resolve_channel_id error: Webshare proxy missing credentials")
        return ""
    if (proxy_type or "").strip().lower() == "generic" and not proxy_candidates:
        print("[CapScript] resolve_channel_id error: Generic proxy URL missing/invalid")
        return ""

    opts = _build_ydl_opts(
        cookies_file=cookies_file,
        cookies_from_browser=cookies_from_browser,
        proxy_type=proxy_type,
        proxy_username=proxy_username,
        proxy_password=proxy_password,
        proxy_url=proxy_url,
        proxy_candidates=proxy_candidates,
        extra_opts={
            "extract_flat": True,
            "playlist_items": "0",
        },
    )
    try:
        if cancel_check:
            cancel_check()
        info = _extract_info_with_proxy_fallback(
            url,
            opts,
            proxy_candidates,
            cancel_check=cancel_check,
        )
        if info:
            for key in ("channel_id", "uploader_id", "id"):
                cid = info.get(key, "")
                if cid and cid.startswith("UC"):
                    return cid
            for entry in (info.get("entries") or []):
                for key in ("channel_id", "uploader_id", "id"):
                    cid = (entry or {}).get(key, "")
                    if cid and cid.startswith("UC"):
                        return cid
    except Exception as e:
        print(f"[CapScript] resolve_channel_id error: {_sanitize_error_message(str(e))}")
    return ""

def format_time(seconds):
    h, remainder = divmod(int(seconds), 3600)
    m, s         = divmod(remainder, 60)
    return f"{h:02d}:{m:02d}:{s:02d}"

def format_views(views):
    return "{:,}".format(int(views))

def _upload_date_to_iso(upload_date: str) -> str:
    if upload_date and len(upload_date) == 8:
        return f"{upload_date[:4]}-{upload_date[4:6]}-{upload_date[6:]}T00:00:00Z"
    return upload_date or ""

def _format_duration(seconds) -> str:
    try:
        s = int(seconds)
        h, rem  = divmod(s, 3600)
        m, sec  = divmod(rem, 60)
        if h:
            return f"{h}h {m:02d}m {sec:02d}s"
        return f"{m}m {sec:02d}s"
    except Exception:
        return "?"

def _response_payload(
    status: str,
    match_count: int = 0,
    results=None,
    structured_results=None,
    output_file=None,
    error: str | None = None,
):
    return {
        "status": status,
        "match_count": int(match_count or 0),
        "results": results or [],
        "structured_results": structured_results or [],
        "output_file": output_file,
        "error": error,
    }

def _build_output_filename(keyword: str, output_filename: str | None) -> str:
    if output_filename:
        base_name = os.path.basename(output_filename.strip())
        safe_name = re.sub(r"[<>:\"/\\|?*\x00-\x1f]", "_", base_name)
        safe_name = safe_name.strip() or "results.txt"
        if not safe_name.lower().endswith(".txt"):
            safe_name += ".txt"
        return safe_name

    safe_kw = re.sub(r"[^\w\s-]", "_", keyword).strip().replace(" ", "_")[:50]
    return f"{safe_kw}_{datetime.now().strftime('%Y%m%d_%H%M%S_%f')}.txt"

def _sanitize_cache_component(value: str) -> str:
    text = (value or "").strip()
    return re.sub(r"[^A-Za-z0-9._-]+", "_", text) or "default"

def _get_transcript_cache_dir() -> str:
    return os.path.join(_get_application_root_path(), _TRANSCRIPT_CACHE_DIRNAME)

def _get_transcript_cache_path(video_id: str, language: str) -> str:
    safe_video_id = _sanitize_cache_component(video_id)
    safe_language = _sanitize_cache_component(language)
    return os.path.join(_get_transcript_cache_dir(), f"{safe_video_id}__{safe_language}.json")

def _load_transcript_cache_entry(video_id: str, language: str):
    cache_path = _get_transcript_cache_path(video_id, language)
    if not os.path.exists(cache_path):
        return None
    try:
        with open(cache_path, "r", encoding="utf-8") as handle:
            payload = json.load(handle)
        if not isinstance(payload, dict):
            return None
        if int(payload.get("schema", 0)) != _TRANSCRIPT_CACHE_SCHEMA:
            return None
        if payload.get("video_id") != video_id:
            return None
        return payload
    except Exception:
        return None

def _load_cached_video_metadata(video_id: str):
    cache_dir = _get_transcript_cache_dir()
    if not os.path.isdir(cache_dir):
        return None

    prefix = f"{_sanitize_cache_component(video_id)}__"
    try:
        for entry_name in os.listdir(cache_dir):
            if not entry_name.startswith(prefix) or not entry_name.endswith(".json"):
                continue
            cache_path = os.path.join(cache_dir, entry_name)
            try:
                with open(cache_path, "r", encoding="utf-8") as handle:
                    payload = json.load(handle)
                if not isinstance(payload, dict):
                    continue
                if payload.get("video_id") != video_id:
                    continue
                metadata = payload.get("metadata") or {}
                if metadata:
                    return metadata
            except Exception:
                continue
    except Exception:
        return None
    return None

def _store_transcript_cache_entry(video_id: str, language: str, payload: dict) -> None:
    if not video_id:
        return
    status = str(payload.get("status") or "").strip().lower()
    if status not in {"ok", "no_transcript"}:
        return

    cache_dir = _get_transcript_cache_dir()
    cache_path = _get_transcript_cache_path(video_id, language)
    os.makedirs(cache_dir, exist_ok=True)

    safe_payload = {
        "schema": _TRANSCRIPT_CACHE_SCHEMA,
        "video_id": video_id,
        "language": language,
        "status": status,
        "cached_at": datetime.utcnow().isoformat(timespec="seconds") + "Z",
        "metadata": payload.get("metadata") or {},
        "transcript": payload.get("transcript") or [],
    }

    tmp_path = f"{cache_path}.tmp"
    try:
        with open(tmp_path, "w", encoding="utf-8") as handle:
            json.dump(safe_payload, handle, ensure_ascii=False)
        os.replace(tmp_path, cache_path)
    except Exception:
        try:
            if os.path.exists(tmp_path):
                os.remove(tmp_path)
        except Exception:
            pass

def _build_cached_video_result(
    vid: str,
    language: str,
    search_pattern,
    log_fn=None,
):
    cached = _load_transcript_cache_entry(vid, language)
    if not cached:
        return None

    metadata = cached.get("metadata") or {}
    status = str(cached.get("status") or "").strip().lower()
    title = metadata.get("title") or vid
    channel = metadata.get("channel") or "Unknown"
    channel_id = metadata.get("channel_id") or ""
    upload_dt = metadata.get("upload_date") or ""
    views = int(metadata.get("views") or 0)
    duration = metadata.get("duration") or 0
    date_short = upload_dt[:10] if upload_dt else "?"

    def _log(msg):
        if log_fn:
            try:
                log_fn(msg)
            except Exception:
                pass

    _log(f"[INFO]  Cache hit — using local transcript for \"{title}\"")
    _log(
        f"[VIDEO] \"{title}\" | {_format_duration(duration)} | "
        f"{format_views(views)} views | {date_short}"
    )

    if status == "no_transcript":
        return {
            "vid": vid,
            "status": "no_transcript",
            "title": title,
        }

    if status != "ok":
        return None

    transcript = cached.get("transcript") or []
    items = [item for item in transcript if search_pattern.search(item.get("text", ""))]
    match_count = len(items)

    if match_count:
        for item in items[:3]:
            ts = format_time(item.get("start", 0))
            preview = item.get("text", "")[:90]
            preview += "…" if len(item.get("text", "")) > 90 else ""
            _log(f"[HIT]   {ts} → {preview}")
        if match_count > 3:
            _log(f"[HIT]   … and {match_count - 3} more")
    else:
        _log(f"[INFO]  No matches ({len(transcript):,} segments scanned)")

    return {
        "vid": vid,
        "status": "ok",
        "title": title,
        "channel": channel,
        "channel_id": channel_id,
        "upload_date": upload_dt,
        "views": views,
        "transcript_len": len(transcript),
        "items": items,
        "match_count": match_count,
    }

def get_video_details(api_key, video_id):
    try:
        cached_metadata = _load_cached_video_metadata(video_id)
        if cached_metadata:
            return {
                "title": cached_metadata.get("title", "Unknown"),
                "channel": cached_metadata.get("channel", "Unknown"),
                "channel_id": cached_metadata.get("channel_id", ""),
                "date": cached_metadata.get("upload_date", ""),
                "views": int(cached_metadata.get("views") or 0),
            }

        _jittered_request_pause(minimum=0.1, maximum=0.5)
        opts = _build_ydl_opts()
        url  = f"https://www.youtube.com/watch?v={video_id}"
        with yt_dlp.YoutubeDL(opts) as ydl:
            info = ydl.extract_info(url, download=False)
        if not info:
            raise ValueError("No info returned")
        return {
            "title":      info.get("title", "Unknown"),
            "channel":    info.get("uploader") or info.get("channel", "Unknown"),
            "channel_id": info.get("channel_id") or info.get("uploader_id", ""),
            "date":       _upload_date_to_iso(info.get("upload_date", "")),
            "views":      int(info.get("view_count") or 0),
        }
    except Exception:
        return {"title": "Unknown", "channel": "Unknown", "channel_id": "", "date": "", "views": 0}

def get_video_details_batch(api_key, video_ids):
    details = {}

    def _fetch_one(vid):
        try:
            cached_metadata = _load_cached_video_metadata(vid)
            if cached_metadata:
                return vid, cached_metadata.get("title", "Title Not Found")

            _jittered_request_pause(minimum=0.1, maximum=0.5)
            opts = _build_ydl_opts()
            url  = f"https://www.youtube.com/watch?v={vid}"
            with yt_dlp.YoutubeDL(opts) as ydl:
                info = ydl.extract_info(url, download=False)
            if info:
                return vid, info.get("title", "Title Not Found")
        except Exception:
            pass
        return vid, "Title Not Found"

    with ThreadPoolExecutor(max_workers=MAX_WORKERS) as ex:
        futures = {ex.submit(_fetch_one, vid): vid for vid in video_ids}
        for fut in as_completed(futures):
            vid, title = fut.result()
            details[vid] = title

    return {vid: details.get(vid, "Title Not Found") for vid in video_ids}

def get_channel_videos(
    api_key,
    channel_id,
    language_code="en",
    max_results=10,
    cookies_file=None,
    cookies_from_browser=None,
    proxy_type=None,
    proxy_username=None,
    proxy_password=None,
    proxy_url=None,
    cancel_check=None,
):
    """
    Return up to max_results * 2 video IDs from a channel (flat, no caption
    pre-check). The main search loop filters videos that end up having no
    transcripts. Over-fetching by 2x compensates for that loss.
    """
    if channel_id.startswith("UC"):
        url = f"https://www.youtube.com/channel/{channel_id}/videos"
    elif channel_id.startswith("@"):
        url = f"https://www.youtube.com/{channel_id}/videos"
    else:
        url = f"https://www.youtube.com/@{channel_id}/videos"

    proxy_candidates = _build_proxy_candidates(
        proxy_type,
        proxy_username,
        proxy_password,
        proxy_url,
    )
    if (proxy_type or "").strip().lower() == "webshare" and not proxy_candidates:
        print("[CapScript] get_channel_videos error: Webshare proxy missing credentials")
        return []
    if (proxy_type or "").strip().lower() == "generic" and not proxy_candidates:
        print("[CapScript] get_channel_videos error: Generic proxy URL missing/invalid")
        return []

    base_opts = _build_ydl_opts(
        cookies_file=cookies_file,
        cookies_from_browser=cookies_from_browser,
        proxy_type=proxy_type,
        proxy_username=proxy_username,
        proxy_password=proxy_password,
        proxy_url=proxy_url,
        proxy_candidates=proxy_candidates,
    )
    base_opts["extract_flat"]   = True
    base_opts["playlist_items"] = f"1:{max_results * 2}"

    video_ids = []
    try:
        if cancel_check:
            cancel_check()
        info = _extract_info_with_proxy_fallback(
            url,
            base_opts,
            proxy_candidates,
            cancel_check=cancel_check,
        )
        for entry in (info or {}).get("entries", []):
            vid = (entry or {}).get("id")
            if vid:
                video_ids.append(vid)
    except Exception as e:
        print(f"[CapScript] get_channel_videos error: {_sanitize_error_message(str(e))}")

    return video_ids

def parse_video_ids(video_ids_input):
    if not video_ids_input:
        return []
    if os.path.isfile(video_ids_input):
        try:
            with open(video_ids_input, "r", encoding="utf-8") as f:
                return _extract_ids_from_text(f.read())
        except Exception:
            return []
    return _extract_ids_from_text(video_ids_input)

def _extract_ids_from_text(text):
    extracted = []
    for item in re.split(r"[,\n]+", text):
        item = item.strip()
        if not item:
            continue
        m = re.search(r"(?:youtube\.com/watch\?.*v=|youtu\.be/)([^&?/\s]+)", item)
        if m:
            extracted.append(m.group(1)); continue
        m = re.search(r"youtube\.com/shorts/([^&?/\s]+)", item)
        if m:
            extracted.append(m.group(1)); continue
        m = re.search(r"youtube\.com/live/([^&?/\s]+)", item)
        if m:
            extracted.append(m.group(1)); continue
        if len(item) == 11 and re.match(r"^[a-zA-Z0-9_-]+$", item):
            extracted.append(item)
    seen, result = set(), []
    for vid in extracted:
        if vid not in seen:
            seen.add(vid)
            result.append(vid)
    return result

def _is_word_char(ch: str) -> bool:
    return bool(ch and re.match(r"\w", ch, flags=re.UNICODE))

def _apply_edge_word_boundaries(pattern_body: str, query: str) -> str:
    if not query:
        return pattern_body

    prefix = r"(?<!\w)" if _is_word_char(query[0]) else ""
    suffix = r"(?!\w)" if _is_word_char(query[-1]) else ""
    return f"{prefix}{pattern_body}{suffix}"

def _build_literal_phrase_pattern(query: str) -> str:
    parts = [re.escape(part) for part in re.split(r"\s+", query.strip()) if part]
    body = r"\s+".join(parts) if parts else re.escape(query.strip())
    return _apply_edge_word_boundaries(body, query.strip())

def _build_token_phrase_pattern(query: str) -> str:
    trimmed = query.strip()
    if not trimmed:
        raise ValueError("keyword is required")

    if _SPECIAL_QUERY_CHAR_RX.search(trimmed):
        return _build_literal_phrase_pattern(trimmed)

    tokens = _SEARCH_TOKEN_RX.findall(trimmed)
    if not tokens:
        return _build_literal_phrase_pattern(trimmed)

    body = r"(?:\W+)".join(re.escape(tok) for tok in tokens)
    return rf"(?<!\w){body}(?!\w)"

def _unwrap_optional_quotes(query: str) -> str:
    text = query.strip()
    if len(text) >= 2 and text[0] == text[-1] and text[0] in ("\"", "'"):
        inner = text[1:-1].strip()
        if inner:
            return inner
    return text

def _build_search_pattern(keyword: str, match_mode: str):
    text = keyword.strip()
    if not text:
        raise ValueError("keyword is required")

    mode = (match_mode or "smart").strip().lower()
    if mode not in _VALID_MATCH_MODES:
        mode = "smart"

    if mode == "contains":
        return re.compile(re.escape(text), re.IGNORECASE), "contains"

    if mode == "exact_phrase":
        phrase = _unwrap_optional_quotes(text)
        pat = _build_token_phrase_pattern(phrase)
        return re.compile(pat, re.IGNORECASE), "exact_phrase"

    unwrapped = _unwrap_optional_quotes(text)
    if unwrapped != text:
        pat = _build_token_phrase_pattern(unwrapped)
        return re.compile(pat, re.IGNORECASE), "exact_phrase"

    pat = _build_token_phrase_pattern(unwrapped)
    return re.compile(pat, re.IGNORECASE), "smart"

def _process_one_video(
    vid,
    language,
    ydl_opts,
    search_pattern,
    log_fn,
    proxy_candidates=None,
    cancel_check=None,
):
    """
    Single entry point for one video: fetch metadata + transcript + search.
    ONE ydl context covers both the extract_info call and the subtitle URL
    fetch, so proxy settings apply to both and subtitle URLs stay valid.

    The _SUBTITLE_SEMAPHORE limits concurrent subtitle HTTP fetches to 3,
    keeping us well under YouTube's timedtext rate limit even at 10 workers.
    """
    url = f"https://www.youtube.com/watch?v={vid}"

    def _log(msg):
        if log_fn:
            try:
                log_fn(msg)
            except Exception:
                pass

    candidates = list(proxy_candidates or [])
    primary_proxy = ydl_opts.get("proxy")
    if primary_proxy and primary_proxy not in candidates:
        candidates.insert(0, primary_proxy)
    if _ydl_uses_cookie_auth(ydl_opts) and None not in candidates:
        candidates.append(None)
    if not candidates:
        candidates = [None]

    info = None
    transcript = None
    title = vid
    channel = "Unknown"
    channel_id = ""
    views = 0
    duration = 0
    upload_dt = ""
    last_error = None

    cached_result = _build_cached_video_result(vid, language, search_pattern, _log)
    if cached_result is not None:
        return cached_result

    for attempt_index, candidate in enumerate(candidates, start=1):
        if cancel_check:
            cancel_check()

        _jittered_request_pause(cancel_check=cancel_check, minimum=0.15, maximum=0.75)

        attempt_opts = dict(ydl_opts)
        attempt_opts["ignoreerrors"] = False

        attempt_opts.pop("format", None)
        attempt_opts.pop("format_sort", None)
        attempt_opts.pop("format_sort_force", None)
        if candidate:
            attempt_opts["proxy"] = candidate
        else:
            attempt_opts.pop("proxy", None)

        candidate_label = _describe_proxy_candidate(candidate)
        if attempt_index > 1:
            _log(
                f"[NET]   Retrying metadata with proxy candidate "
                f"{attempt_index}/{len(candidates)} ({candidate_label})"
            )

        try:
            with yt_dlp.YoutubeDL(attempt_opts) as ydl:
                info = ydl.extract_info(url, download=False)
                if not info:
                    raise RuntimeError("yt-dlp returned no info")

                title = info.get("title", "Unknown")
                channel = info.get("uploader") or info.get("channel", "Unknown")
                channel_id = info.get("channel_id") or info.get("uploader_id", "")
                views = int(info.get("view_count") or 0)
                duration = info.get("duration") or 0
                upload_dt = _upload_date_to_iso(info.get("upload_date", ""))
                date_short = upload_dt[:10] if upload_dt else "?"

                _log(
                    f"[VIDEO] \"{title}\" | "
                    f"{_format_duration(duration)} | "
                    f"{format_views(views)} views | "
                    f"{date_short}"
                )

                with _SUBTITLE_SEMAPHORE:
                    if cancel_check:
                        cancel_check()
                    transcript = _fetch_transcript_json3(
                        info,
                        language,
                        ydl_instance=ydl,
                        log_fn=_log,
                        cancel_check=cancel_check,
                    )
            break
        except Exception as exc:
            last_error = exc
            err_text = _sanitize_error_message(str(exc))
            if "407" in err_text and candidate:
                _log(f"[WARN]  Proxy auth failed on {candidate_label}")
            elif attempt_index < len(candidates):
                _log(f"[NET]   Metadata attempt failed on {candidate_label}")
            continue

    if info is None:
        return {
            "vid": vid,
            "status": "error",
            "title": vid,
            "error": _augment_error_with_restart_hint(
                _sanitize_error_message(str(last_error or "metadata fetch failed"))
            ),
        }

    if transcript is None:
        cache_payload = {
            "status": "no_transcript",
            "metadata": {
                "title": title,
                "channel": channel,
                "channel_id": channel_id,
                "upload_date": upload_dt,
                "views": views,
                "duration": duration,
            },
            "transcript": [],
        }
        _store_transcript_cache_entry(vid, language, cache_payload)
        return {"vid": vid, "status": "no_transcript", "title": title}

    items       = [item for item in transcript if search_pattern.search(item["text"])]
    match_count = len(items)

    if match_count:
        for item in items[:3]:
            ts      = format_time(item["start"])
            preview = item["text"][:90] + ("…" if len(item["text"]) > 90 else "")
            _log(f"[HIT]   {ts} → {preview}")
        if match_count > 3:
            _log(f"[HIT]   … and {match_count - 3} more")
    else:
        _log(f"[INFO]  No matches ({len(transcript):,} segments scanned)")

    _store_transcript_cache_entry(
        vid,
        language,
        {
            "status": "ok",
            "metadata": {
                "title": title,
                "channel": channel,
                "channel_id": channel_id,
                "upload_date": upload_dt,
                "views": views,
                "duration": duration,
            },
            "transcript": transcript,
        },
    )

    return {
        "vid":            vid,
        "status":         "ok",
        "title":          title,
        "channel":        channel,
        "channel_id":     channel_id,
        "upload_date":    upload_dt,
        "views":          views,
        "transcript_len": len(transcript),
        "items":          items,
        "match_count":    match_count,
    }

def search_transcripts(params_json: str, progress_callback=None) -> str:
    try:
        params = json.loads(params_json or "{}")
        if not isinstance(params, dict):
            raise ValueError("params_json must decode to a JSON object")
    except Exception as exc:
        return json.dumps(
            _response_payload(status="error", error=f"Invalid params JSON: {exc}")
        )

    api_key             = params.get("api_key", "")
    search_type         = str(params.get("search_type", "")).strip().lower()
    keyword             = str(params.get("keyword", "")).strip()
    language            = params.get("language", "en")
    output_dir          = params.get("output_dir", "transcripts")
    output_filename     = str(params.get("output_filename") or "").strip()
    requested_match_mode = str(params.get("match_mode") or "smart").strip().lower()
    channel_id          = params.get("channel_id", "")
    video_ids_input     = params.get("video_ids_input", "")
    cookies_file        = params.get("cookies_file") or None
    cookies_from_browser = params.get("cookies_from_browser") or None
    proxy_type          = str(params.get("proxy_type") or "").strip().lower() or None
    proxy_username      = params.get("proxy_username") or None
    proxy_password      = params.get("proxy_password") or None
    proxy_url           = params.get("proxy_url") or None

    if proxy_type == "none":
        proxy_type = None

    try:
        max_results = int(params.get("max_results", 10) or 10)
    except Exception:
        max_results = 10
    max_results = max(1, max_results)

    if search_type not in ("channel", "video"):
        return json.dumps(
            _response_payload(
                status="error",
                error="Invalid search_type; expected 'channel' or 'video'",
            )
        )
    if not keyword:
        return json.dumps(
            _response_payload(status="error", error="keyword is required")
        )

    invalid_mode_requested = bool(requested_match_mode) and requested_match_mode not in _VALID_MATCH_MODES
    try:
        search_pattern, effective_match_mode = _build_search_pattern(
            keyword, requested_match_mode
        )
    except ValueError as exc:
        return json.dumps(_response_payload(status="error", error=str(exc)))

    if proxy_type not in (None, "webshare", "generic"):
        return json.dumps(
            _response_payload(status="error", error=f"Invalid proxy_type '{proxy_type}'")
        )

    proxy_candidates = _build_proxy_candidates(
        proxy_type,
        proxy_username,
        proxy_password,
        proxy_url,
    )

    if proxy_type == "webshare" and not proxy_candidates:
        return json.dumps(
            _response_payload(
                status="error",
                error="Webshare proxy selected but username/password are missing",
            )
        )

    if proxy_type == "generic" and not proxy_candidates:
        return json.dumps(
            _response_payload(
                status="error",
                error="Generic proxy selected but proxy_url is missing or invalid",
            )
        )

    missing_cookies_file = None
    if cookies_file and not os.path.exists(cookies_file):
        missing_cookies_file = cookies_file
        if cookies_from_browser:
            cookies_file = None
        else:
            return json.dumps(
                _response_payload(
                    status="error",
                    error=f"cookies_file not found: {cookies_file}",
                )
            )

    _lock         = threading.Lock()
    _completed    = [0]
    _total_videos = [1]

    def _log(pct, msg):
        if progress_callback:
            try:
                progress_callback(pct, msg)
            except Exception:
                pass

    def _log_msg(msg):
        """Log without updating the progress bar percentage."""
        if progress_callback:
            with _lock:
                pct = int(10 + (_completed[0] / max(1, _total_videos[0])) * 80)
            try:
                progress_callback(pct, msg)
            except Exception:
                pass

    def _check_cancel():
        if not progress_callback:
            return
        with _lock:
            pct = int(10 + (_completed[0] / max(1, _total_videos[0])) * 80)

        progress_callback(pct, "")

    try:
        _check_cancel()

        proxy_info = ""
        if proxy_type and proxy_type != "none":
            proxy_info = f" | proxy: {proxy_type}" + (f" ({proxy_url})" if proxy_url else "")

        cookies_info = ""
        if cookies_file:
            cookies_info = f" | cookies: {os.path.basename(cookies_file)}"
        elif cookies_from_browser:
            cookies_info = f" | cookies-browser: {cookies_from_browser}"

        _log(
            0,
            f"[INFO]  yt-dlp {get_yt_dlp_version()} | {MAX_WORKERS} workers | "
            f"lang: {language}{proxy_info}{cookies_info}",
        )
        if invalid_mode_requested:
            _log(
                0,
                f"[WARN]  Unknown match_mode '{requested_match_mode}', using smart",
            )
        if missing_cookies_file and cookies_from_browser:
            _log(
                0,
                f"[WARN]  cookies_file not found ({missing_cookies_file}); "
                f"falling back to browser cookies ({cookies_from_browser})",
            )

        _log(
            0,
            f"[INFO]  Searching \"{keyword}\" ({search_type}, {effective_match_mode})…",
        )

        vids = []
        if search_type == "channel":
            _check_cancel()
            if channel_id and not channel_id.startswith("UC"):
                _log(3, f"[NET]   Resolving '{channel_id}'…")
                resolved = resolve_channel_id(
                    api_key,
                    channel_id,
                    cookies_file,
                    cookies_from_browser,
                    proxy_type,
                    proxy_username,
                    proxy_password,
                    proxy_url,
                    cancel_check=_check_cancel,
                )
                if resolved:
                    _log(5, f"[NET]   → {resolved}")
                    channel_id = resolved
                else:
                    _log(
                        5,
                        f"[WARN]  Could not resolve channel '{channel_id}', "
                        "trying direct handle/slug",
                    )

            _log(5, "[NET]   Fetching video list…")
            vids = get_channel_videos(
                api_key,
                channel_id,
                language,
                max_results,
                cookies_file,
                cookies_from_browser,
                proxy_type,
                proxy_username,
                proxy_password,
                proxy_url,
                cancel_check=_check_cancel,
            )
            _log(
                10,
                f"[STAT]  {len(vids)} videos retrieved → processing "
                f"{min(len(vids), max_results)}",
            )
        else:
            _check_cancel()
            vids = parse_video_ids(video_ids_input)
            _log(10, f"[STAT]  {len(vids)} video IDs parsed")

        if not vids:
            _log(100, "[WARN]  No videos found to process")
            return json.dumps(_response_payload(status="ok"))

        vids             = vids[:max_results]
        _total_videos[0] = len(vids)

        ydl_opts = _build_ydl_opts(
            language=language,
            cookies_file=cookies_file,
            cookies_from_browser=cookies_from_browser,
            proxy_type=proxy_type,
            proxy_username=proxy_username,
            proxy_password=proxy_password,
            proxy_url=proxy_url,
            proxy_candidates=proxy_candidates,
        )

        video_results = {}
        cancelled = False

        def _worker(vid):
            return vid, _process_one_video(
                vid,
                language,
                ydl_opts,
                search_pattern,
                _log_msg,
                proxy_candidates,
                cancel_check=_check_cancel,
            )

        futures = {}
        executor = ThreadPoolExecutor(max_workers=MAX_WORKERS)
        try:
            _check_cancel()
            futures = {executor.submit(_worker, vid): vid for vid in vids}
            for fut in as_completed(futures):
                _check_cancel()
                vid = futures[fut]
                try:
                    result_vid, result = fut.result()
                    if result_vid:
                        vid = result_vid
                except KeyboardInterrupt:
                    cancelled = True
                    break
                except Exception as exc:
                    result = {
                        "vid": vid,
                        "status": "error",
                        "title": vid,
                        "error": _augment_error_with_restart_hint(_sanitize_error_message(str(exc))),
                    }

                video_results[vid] = result
                with _lock:
                    _completed[0] += 1
                    done  = _completed[0]
                    total = _total_videos[0]
                    pct   = int(10 + (done / total) * 80)

                status = result.get("status", "?")
                title  = result.get("title", vid)
                if status == "ok":
                    mc  = result.get("match_count", 0)
                    hit = f"{mc} match{'es' if mc != 1 else ''}" if mc else "no matches"
                    _log(pct, f"[STAT]  [{done}/{total}] \"{title}\" — {hit}")
                elif status == "no_transcript":
                    _log(pct, f"[WARN]  [{done}/{total}] No transcript — \"{title}\"")
                else:
                    _log(pct, f"[ERR]   [{done}/{total}] {vid}: {result.get('error', '')}")
        except KeyboardInterrupt:
            cancelled = True
        finally:
            if cancelled:
                for fut in futures:
                    fut.cancel()
                executor.shutdown(wait=False, cancel_futures=True)
            else:
                executor.shutdown(wait=True)

        matched_results = [
            r
            for r in video_results.values()
            if r.get("status") == "ok" and r.get("match_count", 0) > 0
        ]
        matched_results.sort(key=lambda r: r["match_count"], reverse=True)
        total_matches = sum(r["match_count"] for r in matched_results)

        results_text = []
        structured_results = []
        for r in matched_results:
            vid   = r["vid"]
            items = r["items"]

            block = (
                f"Video Title: {r['title']}\n"
                f"Video ID:    {vid}\n"
                f"Channel:     {r['channel']} ({r['channel_id']})\n"
                f"Date:        {r['upload_date']}\n"
                f"Views:       {format_views(r['views'])}\n"
                f"Matches:     {r['match_count']} in {r['transcript_len']:,} segments\n"
                "Timestamps:\n"
            )

            structured_matches = []
            for item in items:
                timestamp = format_time(item["start"])
                block += f"╳ {timestamp} - {item['text']}\n"
                structured_matches.append(
                    {
                        "start_seconds": item["start"],
                        "timestamp": timestamp,
                        "text": item["text"],
                    }
                )

            block += "\n" + "═" * 40 + "\n\n"
            results_text.append(block)

            structured_results.append(
                {
                    "video_id": vid,
                    "title": r["title"],
                    "channel": r["channel"],
                    "channel_id": r["channel_id"],
                    "upload_date": r["upload_date"],
                    "views": r["views"],
                    "match_count": r["match_count"],
                    "transcript_segments": r["transcript_len"],
                    "matches": structured_matches,
                }
            )

        no_transcript = sum(
            1 for r in video_results.values() if r.get("status") == "no_transcript"
        )
        errors = sum(
            1 for r in video_results.values() if r.get("status") == "error"
        )

        if no_transcript:
            _log(
                95,
                f"[WARN]  {no_transcript} video{'s' if no_transcript != 1 else ''} "
                "had no transcript",
            )
        if errors:
            _log(95, f"[ERR]   {errors} video{'s' if errors != 1 else ''} errored")

        output_file = None
        if total_matches > 0 and not cancelled:
            try:
                os.makedirs(output_dir, exist_ok=True)
                fname = _build_output_filename(keyword, output_filename)
                output_file = os.path.join(output_dir, fname)
                with open(output_file, "w", encoding="utf-8") as f:
                    f.write("\n".join(results_text))
                _log(98, f"[INFO]  Results saved → {output_file}")
            except Exception as exc:
                _log(98, f"[WARN]  Could not save output: {exc}")
                output_file = None

        if cancelled:
            _log(
                100,
                f"[WARN]  Cancelled — {total_matches} match"
                f"{'es' if total_matches != 1 else ''} captured before stop",
            )
            return json.dumps(
                _response_payload(
                    status="cancelled",
                    match_count=total_matches,
                    results=results_text,
                    structured_results=structured_results,
                    output_file=output_file,
                )
            )

        _log(
            100,
            f"[STAT]  Done — {total_matches} match{'es' if total_matches != 1 else ''} "
            f"in {len(matched_results)} video{'s' if len(matched_results) != 1 else ''}",
        )
        return json.dumps(
            _response_payload(
                status="ok",
                match_count=total_matches,
                results=results_text,
                structured_results=structured_results,
                output_file=output_file,
            )
        )

    except KeyboardInterrupt:

        try:
            if progress_callback:
                progress_callback(100, "[WARN]  Search cancelled by user")
        except BaseException:
            pass
        return json.dumps(_response_payload(status="cancelled"))
    except Exception as exc:
        try:
            if progress_callback:
                progress_callback(100, f"[ERR]   Search failed: {exc}")
        except BaseException:
            pass
        return json.dumps(_response_payload(status="error", error=str(exc)))

def _build_channel_base_urls(channel_input: str):
    raw = (channel_input or "").strip()
    if not raw:
        return []

    urls = []

    def _add(url: str):
        normalized = (url or "").strip().rstrip("/")
        if normalized and normalized not in urls:
            urls.append(normalized)

    if raw.startswith("http://") or raw.startswith("https://"):
        parsed = urllib.parse.urlsplit(raw)
        base_path = (parsed.path or "").rstrip("/")
        for tab in ("/videos", "/streams", "/shorts", "/search"):
            if base_path.endswith(tab):
                base_path = base_path[: -len(tab)]
                break

        if parsed.netloc:
            _add(
                urllib.parse.urlunsplit(
                    (parsed.scheme or "https", parsed.netloc, base_path, "", "")
                )
            )

        parts = [part for part in base_path.split("/") if part]
        if parts:
            if parts[0].startswith("@"):
                _add(f"https://www.youtube.com/{parts[0]}")
            elif len(parts) >= 2 and parts[0] in ("channel", "c", "user"):
                _add(f"https://www.youtube.com/{parts[0]}/{parts[1]}")

        return urls

    if raw.startswith("UC") and len(raw) >= 24:
        _add(f"https://www.youtube.com/channel/{raw}")
        return urls

    if raw.startswith("@"):
        _add(f"https://www.youtube.com/{raw}")
        return urls

    _add(f"https://www.youtube.com/@{raw}")
    _add(f"https://www.youtube.com/c/{raw}")
    _add(f"https://www.youtube.com/user/{raw}")
    return urls

def fetch_videos_by_channel_date(
    api_key,
    channel_id,
    start_iso,
    end_iso,
    cookies_file=None,
    cookies_from_browser=None,
    proxy_type=None,
    proxy_username=None,
    proxy_password=None,
    proxy_url=None,
    cancel_check=None,
):
    from yt_dlp.utils import DateRange

    def _iso_to_ydl(iso):
        try:
            return iso[:10].replace("-", "") if iso else None
        except Exception:
            return None

    start_str = _iso_to_ydl(start_iso)
    end_str   = _iso_to_ydl(end_iso)

    base_urls = _build_channel_base_urls(channel_id)
    if not base_urls:
        return []

    proxy_candidates = _build_proxy_candidates(
        proxy_type,
        proxy_username,
        proxy_password,
        proxy_url,
    )
    if (proxy_type or "").strip().lower() == "webshare" and not proxy_candidates:
        print("[CapScript] search_videos_by_keyword error: Webshare proxy missing credentials")
        return []
    if (proxy_type or "").strip().lower() == "generic" and not proxy_candidates:
        print("[CapScript] search_videos_by_keyword error: Generic proxy URL missing/invalid")
        return []

    opts = _build_ydl_opts(
        cookies_file=cookies_file,
        cookies_from_browser=cookies_from_browser,
        proxy_type=proxy_type,
        proxy_username=proxy_username,
        proxy_password=proxy_password,
        proxy_url=proxy_url,
        proxy_candidates=proxy_candidates,
        extra_opts={
            "extract_flat": True,
            "ignoreerrors": True,
        },
    )
    if start_str and end_str:
        opts["daterange"] = DateRange(start_str, end_str)

    def _extract_video_entries(url: str):
        if cancel_check:
            cancel_check()
        info = _extract_info_with_proxy_fallback(
            url,
            opts,
            proxy_candidates,
            cancel_check=cancel_check,
        )
        videos_local = []
        seen = set()
        for entry in (info or {}).get("entries", []):
            vid = (entry or {}).get("id")
            title = (entry or {}).get("title", "No Title")
            if vid and vid not in seen:
                seen.add(vid)
                videos_local.append({"id": vid, "title": title})
        return videos_local

    last_error = None
    for base_url in base_urls:
        try:
            videos = _extract_video_entries(f"{base_url}/videos")
            if videos:
                return videos
        except Exception as e:
            last_error = e
            err_text = _sanitize_error_message(str(e))
            if "429" in err_text:
                _cooperative_sleep(0.8 + random.uniform(0.2, 0.6), cancel_check)

        fallback_videos = []
        fallback_seen = set()
        for suffix in ("/streams", "/shorts"):
            try:
                videos = _extract_video_entries(f"{base_url}{suffix}")
            except Exception as e:
                last_error = e
                err_text = _sanitize_error_message(str(e))
                if "429" in err_text:
                    _cooperative_sleep(0.8 + random.uniform(0.2, 0.6), cancel_check)
                continue

            for video in videos:
                vid = video.get("id")
                if vid and vid not in fallback_seen:
                    fallback_seen.add(vid)
                    fallback_videos.append(video)

        if fallback_videos:
            return fallback_videos

    if last_error is not None:
        print(
            f"[CapScript] fetch_videos_by_channel_date error: "
            f"{_sanitize_error_message(str(last_error))}"
        )
    return []

def search_videos_by_keyword(
    api_key,
    channel_id,
    keyword,
    start_iso=None,
    end_iso=None,
    cookies_file=None,
    cookies_from_browser=None,
    proxy_type=None,
    proxy_username=None,
    proxy_password=None,
    proxy_url=None,
    cancel_check=None,
):
    from yt_dlp.utils import DateRange

    def _iso_to_ydl(iso):
        try:
            return iso[:10].replace("-", "") if iso else None
        except Exception:
            return None

    start_str = _iso_to_ydl(start_iso)
    end_str   = _iso_to_ydl(end_iso)

    base_urls = _build_channel_base_urls(channel_id) if channel_id else []
    search_urls = [f"{base}/search?query={urllib.parse.quote(keyword)}" for base in base_urls]
    if not search_urls:
        search_urls = [f"ytsearch50:{keyword}"]

    proxy_candidates = _build_proxy_candidates(
        proxy_type,
        proxy_username,
        proxy_password,
        proxy_url,
    )
    if (proxy_type or "").strip().lower() == "webshare" and not proxy_candidates:
        print("[CapScript] search_videos_by_keyword error: Webshare proxy missing credentials")
        return []
    if (proxy_type or "").strip().lower() == "generic" and not proxy_candidates:
        print("[CapScript] search_videos_by_keyword error: Generic proxy URL missing/invalid")
        return []

    opts = _build_ydl_opts(
        cookies_file=cookies_file,
        cookies_from_browser=cookies_from_browser,
        proxy_type=proxy_type,
        proxy_username=proxy_username,
        proxy_password=proxy_password,
        proxy_url=proxy_url,
        proxy_candidates=proxy_candidates,
        extra_opts={
            "extract_flat": True,
            "ignoreerrors": True,
        },
    )
    if start_str and end_str:
        opts["daterange"] = DateRange(start_str, end_str)

    keyword_lower = keyword.lower()
    last_error = None

    def _extract_and_filter(url: str, seen: set):
        videos = []
        info = _extract_info_with_proxy_fallback(
            url,
            opts,
            proxy_candidates,
            cancel_check=cancel_check,
        )
        for entry in (info or {}).get("entries", []):
            vid = (entry or {}).get("id")
            title = (entry or {}).get("title", "")
            if vid and title and keyword_lower in title.lower() and vid not in seen:
                seen.add(vid)
                videos.append({"id": vid, "title": title})
        return videos

    for url in search_urls:
        videos = []
        seen = set()
        try:
            if cancel_check:
                cancel_check()
            videos = _extract_and_filter(url, seen)
            if videos:
                return videos
        except Exception as e:
            last_error = e
            err_text = _sanitize_error_message(str(e))
            if "429" in err_text:
                _cooperative_sleep(0.8 + random.uniform(0.2, 0.6), cancel_check)
            continue

    if base_urls:
        for base_url in base_urls:
            fallback_videos = []
            seen = set()
            for suffix in ("/videos", "/streams", "/shorts"):
                try:
                    if cancel_check:
                        cancel_check()
                    videos = _extract_and_filter(f"{base_url}{suffix}", seen)
                    if videos:
                        fallback_videos.extend(videos)
                except Exception as e:
                    last_error = e
                    err_text = _sanitize_error_message(str(e))
                    if "429" in err_text:
                        _cooperative_sleep(0.8 + random.uniform(0.2, 0.6), cancel_check)
                    continue
            if fallback_videos:
                return fallback_videos

    if last_error is not None:
        print(f"[CapScript] search_videos_by_keyword error: {_sanitize_error_message(str(last_error))}")
    return []