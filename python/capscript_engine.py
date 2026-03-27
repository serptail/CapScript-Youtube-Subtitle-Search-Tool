import os
import sys
import re
import json
import time
import base64
import configparser
from datetime import datetime, timedelta
from concurrent.futures import ThreadPoolExecutor, as_completed


from youtube_transcript_api import YouTubeTranscriptApi
from youtube_transcript_api._errors import NoTranscriptFound, TranscriptsDisabled
from googleapiclient.discovery import build
from googleapiclient.errors import HttpError
from cryptography.fernet import Fernet
from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.primitives.kdf.pbkdf2 import PBKDF2HMAC


MAX_WORKERS = 10


def _get_application_root_path():

    if getattr(sys, "frozen", False):
        return os.path.dirname(sys.executable)
    return os.path.dirname(os.path.abspath(__file__))


PREFERENCES_FILE_PATH = os.path.join(_get_application_root_path(), "preferences.ini")
_PREFERENCES_SECTION = "Preferences"
_API_KEY_OPTION = "API_KEY"


_PROXY_SECTION = "Proxy"
_PROXY_TYPE_OPTION = "type"
_PROXY_USERNAME_OPTION = "username"
_PROXY_PASSWORD_OPTION = "password"
_PROXY_URL_OPTION = "url"


def _get_ytt_api(
    cookies_file=None,
    proxy_type=None,
    proxy_username=None,
    proxy_password=None,
    proxy_url=None,
):
    """
    Return a YouTubeTranscriptApi instance.

    Supports:
      - Cookie file for authentication (Netscape / Mozilla format)
      - Webshare rotating residential proxy (username + password)
      - Generic HTTP/HTTPS/SOCKS proxy (URL)

    If `cookies_file` is provided and exists, loads it into a requests
    Session for authenticated requests.
    """
    import requests

    proxy_config = None
    http_client = None

    if proxy_type == "webshare" and proxy_username and proxy_password:
        try:
            from youtube_transcript_api.proxies import WebshareProxyConfig

            proxy_config = WebshareProxyConfig(
                proxy_username=proxy_username,
                proxy_password=proxy_password,
            )
            print(f"[CapScript] Using Webshare proxy (user={proxy_username})")
        except ImportError:
            print(
                "[CapScript] WebshareProxyConfig not available in this "
                "youtube-transcript-api version."
            )
    elif proxy_type == "generic" and proxy_url:
        try:
            from youtube_transcript_api.proxies import GenericProxyConfig

            proxy_config = GenericProxyConfig(
                http_url=proxy_url,
                https_url=proxy_url,
            )
            print(f"[CapScript] Using generic proxy: {proxy_url}")
        except ImportError:
            print(
                "[CapScript] GenericProxyConfig not available in this "
                "youtube-transcript-api version."
            )

    if not cookies_file:
        default = os.path.join(_get_application_root_path(), "cookies.txt")
        if os.path.exists(default):
            cookies_file = default

    if cookies_file and os.path.exists(cookies_file):
        try:
            import http.cookiejar

            jar = http.cookiejar.MozillaCookieJar(cookies_file)
            jar.load(ignore_discard=True, ignore_expires=True)
            session = requests.Session()
            session.cookies = jar
            http_client = session
            print(f"[CapScript] Loaded cookies from: {cookies_file}")
        except Exception as e:
            print(f"[CapScript] Failed to load cookies: {e}")

    kwargs = {}
    if proxy_config is not None:
        kwargs["proxy_config"] = proxy_config
    if http_client is not None:
        kwargs["http_client"] = http_client
    return YouTubeTranscriptApi(**kwargs)


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
        _PROXY_TYPE_OPTION: proxy_type or "none",
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

    config = configparser.ConfigParser()
    defaults = {"type": "none", "username": "", "password": "", "url": ""}
    if not os.path.exists(PREFERENCES_FILE_PATH):
        return json.dumps(defaults)
    try:
        config.read(PREFERENCES_FILE_PATH, encoding="utf-8")
        proxy_type = config.get(_PROXY_SECTION, _PROXY_TYPE_OPTION, fallback="none")
        proxy_username = config.get(_PROXY_SECTION, _PROXY_USERNAME_OPTION, fallback="")
        enc_password = config.get(_PROXY_SECTION, _PROXY_PASSWORD_OPTION, fallback="")
        proxy_password = _decrypt_api_key(enc_password) if enc_password else ""
        proxy_url = config.get(_PROXY_SECTION, _PROXY_URL_OPTION, fallback="")
        return json.dumps(
            {
                "type": proxy_type,
                "username": proxy_username,
                "password": proxy_password,
                "url": proxy_url,
            }
        )
    except Exception:
        return json.dumps(defaults)


def _get_encryption_key():
    machine_id = os.environ.get("COMPUTERNAME", "default") + os.environ.get(
        "USERNAME", "user"
    )
    salt = machine_id.encode()[:16].ljust(16, b"0")
    kdf = PBKDF2HMAC(algorithm=hashes.SHA256(), length=32, salt=salt, iterations=100000)
    return base64.urlsafe_b64encode(kdf.derive(b"CapScriptProAPIKey2025"))


def _encrypt_api_key(api_key):
    try:
        if not api_key:
            return ""
        fernet = Fernet(_get_encryption_key())
        encrypted = fernet.encrypt(api_key.encode())
        return base64.urlsafe_b64encode(encrypted).decode()
    except Exception:
        return api_key


def _decrypt_api_key(encrypted_key):
    try:
        if not encrypted_key:
            return ""
        fernet = Fernet(_get_encryption_key())
        encrypted_bytes = base64.urlsafe_b64decode(encrypted_key.encode())
        return fernet.decrypt(encrypted_bytes).decode()
    except Exception:
        return encrypted_key


def validate_api_key(api_key: str) -> bool:

    try:
        youtube = build("youtube", "v3", developerKey=api_key)
        youtube.search().list(part="id", maxResults=1, q="test").execute()
        return True
    except Exception:
        return False


def save_api_key(api_key: str) -> bool:

    config = configparser.ConfigParser()
    directory = os.path.dirname(PREFERENCES_FILE_PATH)
    if directory:
        os.makedirs(directory, exist_ok=True)
    encrypted = _encrypt_api_key(api_key)
    config[_PREFERENCES_SECTION] = {_API_KEY_OPTION: encrypted}
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


def resolve_channel_id(api_key: str, channel_input: str) -> str:
    """
    Resolve a YouTube channel URL, @handle, or custom URL to a UC... channel ID.

    Accepts:
      - UCxxxxxxxx (returned as-is)
      - @handle or youtube.com/@handle
      - youtube.com/channel/UCxxxxxxxx
      - youtube.com/c/CustomName
      - Plain custom name

    Returns the UC... channel ID, or empty string on failure.
    """
    channel_input = channel_input.strip()
    if not channel_input:
        return ""

    if channel_input.startswith("UC") and len(channel_input) >= 24:
        return channel_input

    import re as _re

    m = _re.search(r"youtube\.com/channel/(UC[\w-]{22,})", channel_input)
    if m:
        return m.group(1)

    handle = None
    m = _re.search(r"youtube\.com/@([\w.-]+)", channel_input)
    if m:
        handle = m.group(1)
    elif channel_input.startswith("@"):
        handle = channel_input[1:]

    custom_name = None
    m = _re.search(r"youtube\.com/c/([\w.-]+)", channel_input)
    if m:
        custom_name = m.group(1)

    youtube = _get_service(api_key)

    if handle:
        try:
            resp = youtube.channels().list(part="id", forHandle=handle).execute()
            items = resp.get("items", [])
            if items:
                return items[0]["id"]
        except Exception:
            pass

    name_to_try = handle or custom_name or channel_input
    try:
        resp = youtube.channels().list(part="id", forUsername=name_to_try).execute()
        items = resp.get("items", [])
        if items:
            return items[0]["id"]
    except Exception:
        pass

    try:
        resp = (
            youtube.search()
            .list(part="snippet", q=name_to_try, type="channel", maxResults=1)
            .execute()
        )
        items = resp.get("items", [])
        if items:
            return items[0]["snippet"]["channelId"]
    except Exception:
        pass

    return ""


def _get_service(api_key):
    return build("youtube", "v3", developerKey=api_key)


def format_time(seconds):
    h, remainder = divmod(int(seconds), 3600)
    m, s = divmod(remainder, 60)
    return f"{h:02d}:{m:02d}:{s:02d}"


def format_views(views):
    return "{:,}".format(int(views))


def has_captions(
    video_id,
    language_code,
    cookies_file=None,
    proxy_type=None,
    proxy_username=None,
    proxy_password=None,
    proxy_url=None,
):
    try:
        ytt_api = _get_ytt_api(
            cookies_file, proxy_type, proxy_username, proxy_password, proxy_url
        )
        transcript_list = ytt_api.list(video_id)
        transcript_list.find_transcript([language_code])
        return True
    except Exception:
        return False


def get_video_details(api_key, video_id):

    try:
        youtube = _get_service(api_key)
        resp = youtube.videos().list(part="snippet,statistics", id=video_id).execute()
        if not resp.get("items"):
            return {
                "title": "Unknown",
                "channel": "Unknown",
                "channel_id": "",
                "date": "",
                "views": 0,
            }
        info = resp["items"][0]["snippet"]
        stats = resp["items"][0].get("statistics", {})
        return {
            "title": info.get("title", "Unknown"),
            "channel": info.get("channelTitle", "Unknown"),
            "channel_id": info.get("channelId", ""),
            "date": info.get("publishedAt", ""),
            "views": int(stats.get("viewCount", 0)),
        }
    except Exception:
        return {
            "title": "Unknown",
            "channel": "Unknown",
            "channel_id": "",
            "date": "",
            "views": 0,
        }


def get_channel_videos(
    api_key,
    channel_id,
    language_code="en",
    max_results=10,
    cookies_file=None,
    proxy_type=None,
    proxy_username=None,
    proxy_password=None,
    proxy_url=None,
):

    youtube = _get_service(api_key)
    video_ids = []
    next_page = None
    fetched = 0
    while fetched < max_results:
        try:
            resp = (
                youtube.search()
                .list(
                    part="id",
                    channelId=channel_id,
                    type="video",
                    maxResults=min(50, max_results * 2),
                    order="date",
                    pageToken=next_page,
                )
                .execute()
            )
            items = resp.get("items", [])
            if not items and not next_page:
                break
            for item in items:
                vid = item["id"].get("videoId")
                if (
                    vid
                    and vid not in video_ids
                    and has_captions(
                        vid,
                        language_code,
                        cookies_file,
                        proxy_type,
                        proxy_username,
                        proxy_password,
                        proxy_url,
                    )
                ):
                    video_ids.append(vid)
                    fetched += 1
                    if fetched >= max_results:
                        break
            next_page = resp.get("nextPageToken")
            if not next_page:
                break
        except Exception:
            break
    return video_ids[:max_results]


def parse_video_ids(video_ids_input):

    if not video_ids_input:
        return []

    if os.path.isfile(video_ids_input):
        try:
            with open(video_ids_input, "r", encoding="utf-8") as f:
                content = f.read()
                return _extract_ids_from_text(content)
        except Exception:
            return []

    return _extract_ids_from_text(video_ids_input)


def _extract_ids_from_text(text):

    extracted = []

    items = re.split(r"[,\n]+", text)

    for item in items:
        item = item.strip()
        if not item:
            continue

        match_watch = re.search(
            r"(?:youtube\.com/watch\?.*v=|youtu\.be/)([^&?/\s]+)", item
        )
        if match_watch:
            extracted.append(match_watch.group(1))
            continue

        match_shorts = re.search(r"youtube\.com/shorts/([^&?/\s]+)", item)
        if match_shorts:
            extracted.append(match_shorts.group(1))
            continue

        match_live = re.search(r"youtube\.com/live/([^&?/\s]+)", item)
        if match_live:
            extracted.append(match_live.group(1))
            continue

        if len(item) == 11 and re.match(r"^[a-zA-Z0-9_-]+$", item):
            extracted.append(item)

    seen = set()
    result = []
    for vid in extracted:
        if vid not in seen:
            seen.add(vid)
            result.append(vid)

    return result


def search_transcripts(params_json: str, progress_callback=None) -> str:

    params = json.loads(params_json)
    api_key = params["api_key"]
    search_type = params["search_type"]
    keyword = params["keyword"]
    language = params.get("language", "en")
    output_dir = params.get("output_dir", "transcripts")
    channel_id = params.get("channel_id", "")
    max_results = params.get("max_results", 10)
    video_ids_input = params.get("video_ids_input", "")
    cookies_file = params.get("cookies_file", None) or None

    proxy_type = params.get("proxy_type", None) or None
    proxy_username = params.get("proxy_username", None) or None
    proxy_password = params.get("proxy_password", None) or None
    proxy_url = params.get("proxy_url", None) or None

    print(
        f"[CapScript] API key: {'set (' + api_key[:8] + '...)' if api_key else 'NOT SET'}"
    )
    print(f"[CapScript] Proxy type: {proxy_type or 'none'}")
    if cookies_file:
        print(f"[CapScript] Cookies file: {cookies_file}")

    match_count = 0
    results = []
    search_pattern = re.compile(r"\b" + re.escape(keyword) + r"\b", re.IGNORECASE)

    def _log(pct, msg):
        if progress_callback:
            try:
                progress_callback(pct, msg)
            except Exception:
                pass

    _log(0, f"Starting search for '{keyword}'...")

    youtube = _get_service(api_key)
    vids = []
    if search_type == "channel":

        if channel_id and not channel_id.startswith("UC"):
            _log(3, f"Resolving channel '{channel_id}'...")
            resolved = resolve_channel_id(api_key, channel_id)
            if resolved:
                _log(5, f"Resolved to {resolved}")
                channel_id = resolved
            else:
                _log(100, f"Could not resolve channel '{channel_id}'.")
                return json.dumps(
                    {"match_count": 0, "results": [], "output_file": None}
                )
        _log(5, f"Fetching channel videos for '{channel_id}'...")
        vids = get_channel_videos(
            api_key,
            channel_id,
            language,
            max_results,
            cookies_file,
            proxy_type,
            proxy_username,
            proxy_password,
            proxy_url,
        )
        _log(10, f"Found {len(vids)} videos with captions.")
    else:
        vids = parse_video_ids(video_ids_input)
        _log(10, f"Parsed {len(vids)} video IDs.")

    if not vids:
        _log(100, "No videos found to process.")
        return json.dumps({"match_count": 0, "results": [], "output_file": None})

    total = len(vids)
    for i, vid in enumerate(vids, 1):
        _log(int(10 + (i / total) * 80), f"Processing video {i}/{total} ({vid})...")
        try:
            ytt_api = _get_ytt_api(
                cookies_file, proxy_type, proxy_username, proxy_password, proxy_url
            )
            fetched_transcript = ytt_api.fetch(vid, languages=[language])
            transcript = fetched_transcript.to_raw_data()
            items = [item for item in transcript if search_pattern.search(item["text"])]

            if items:
                current = len(items)
                match_count += current
                details = get_video_details(api_key, vid)
                block = f"Video Title: {details['title']}\n"
                block += f"Video ID: {vid}\n"
                block += f"Channel: {details['channel']} ({details['channel_id']})\n"
                block += f"Date: {details['date']}\n"
                block += f"Views: {format_views(details['views'])}\n"
                block += "Timestamps:\n"
                for item in items:
                    ts = format_time(item["start"])
                    block += f"╳ {ts} - {item['text']}\n"
                block += "\n" + "═" * 40 + "\n\n"
                results.append(block)
        except (NoTranscriptFound, TranscriptsDisabled):
            _log(int(10 + (i / total) * 80), f"No transcript for {vid}.")
        except Exception as e:
            _log(int(10 + (i / total) * 80), f"Error on {vid}: {e}")

    output_file = None
    if match_count > 0:
        try:
            os.makedirs(output_dir, exist_ok=True)
            safe_kw = re.sub(r"[^\w\s-]", "_", keyword).strip().replace(" ", "_")[:50]
            fname = f"{safe_kw}_{datetime.now().strftime('%Y%m%d_%H%M%S')}.txt"
            output_file = os.path.join(output_dir, fname)
            with open(output_file, "w", encoding="utf-8") as f:
                f.write("\n".join(results))
        except Exception:
            output_file = None

    _log(100, f"Done. {match_count} matches found.")
    return json.dumps(
        {
            "match_count": match_count,
            "results": results,
            "output_file": output_file,
        }
    )


def fetch_videos_by_channel_date(api_key, channel_id, start_iso, end_iso):

    youtube = _get_service(api_key)
    videos = []
    next_page = None
    try:
        while True:
            resp = (
                youtube.search()
                .list(
                    part="snippet",
                    channelId=channel_id,
                    maxResults=50,
                    order="date",
                    type="video",
                    publishedAfter=start_iso,
                    publishedBefore=end_iso,
                    pageToken=next_page,
                )
                .execute()
            )
            for item in resp.get("items", []):
                vid = item.get("id", {}).get("videoId")
                title = item.get("snippet", {}).get("title", "No Title")
                if vid:
                    videos.append({"id": vid, "title": title})
            next_page = resp.get("nextPageToken")
            if not next_page:
                break
    except Exception:
        pass
    return videos


def search_videos_by_keyword(
    api_key, channel_id, keyword, start_iso=None, end_iso=None
):

    youtube = _get_service(api_key)
    videos = []
    next_page = None
    keyword_lower = keyword.lower()
    try:
        for _ in range(5):
            params = {
                "part": "snippet",
                "q": keyword,
                "maxResults": 50,
                "order": "relevance",
                "type": "video",
                "pageToken": next_page,
            }
            if channel_id:
                params["channelId"] = channel_id
            if start_iso:
                params["publishedAfter"] = start_iso
            if end_iso:
                params["publishedBefore"] = end_iso
            resp = youtube.search().list(**params).execute()
            for item in resp.get("items", []):
                vid = item.get("id", {}).get("videoId")
                title = item.get("snippet", {}).get("title", "")
                if vid and title:
                    videos.append({"id": vid, "title": title})
            next_page = resp.get("nextPageToken")
            if not next_page:
                break
    except Exception:
        pass
    return [v for v in videos if keyword_lower in v["title"].lower()]


def get_video_details_batch(api_key, video_ids):

    youtube = _get_service(api_key)
    details = {}
    try:
        for i in range(0, len(video_ids), 50):
            batch = video_ids[i : i + 50]
            resp = youtube.videos().list(part="snippet", id=",".join(batch)).execute()
            for item in resp.get("items", []):
                vid = item.get("id")
                title = item.get("snippet", {}).get("title", "Unknown")
                if vid:
                    details[vid] = title
    except Exception:
        pass
    return {vid: details.get(vid, "Title Not Found") for vid in video_ids}
