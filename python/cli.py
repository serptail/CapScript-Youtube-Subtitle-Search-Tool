import sys
import json
import time
import argparse
import threading
from collections import deque

from rich.live    import Live
from rich.text    import Text
from rich.console import Console
from rich.theme   import Theme

import os
import importlib.util as _ilu

def _load_engine():
    engine_path = os.path.join(
        os.path.dirname(os.path.abspath(__file__)), "capscript_engine.py"
    )
    engine_path = os.path.normpath(engine_path)
    if not os.path.exists(engine_path):
        sys.exit(
            f"[ERROR] capscript_engine.py not found at: {engine_path}\n"
            "Make sure cli.py is one level below capscript_engine.py."
        )
    spec = _ilu.spec_from_file_location("capscript_engine", engine_path)
    mod  = _ilu.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod

eng = _load_engine()

theme = Theme({
    "info":    "dim red",
    "success": "bold green",
    "warn":    "bold yellow",
    "error":   "bold red",
    "accent":  "red",
    "muted":   "grey46",
    "bracket": "dark_red",
    "live_msg":"color(124)",
    "label":   "color(196)",
})
console = Console(theme=theme, highlight=False)

BAR_SEGMENTS = 40
DASH         = "-"

def _render_bar(pct: float) -> Text:
    filled = int(pct / 100 * BAR_SEGMENTS)
    empty  = BAR_SEGMENTS - filled
    t = Text(no_wrap=True)

    for _ in range(filled):
        t.append(DASH, style="bold green")

    for _ in range(empty):
        t.append(DASH, style="white")

    return t

class _SearchState:

    LOG_LINES = 6

    def __init__(self):
        self._lock       = threading.Lock()
        self.pct         = 0
        self.match_count = 0
        self.start_time  = time.monotonic()
        self._log        = deque(maxlen=self.LOG_LINES)

    def push(self, pct: int, msg: str) -> None:
        with self._lock:
            self.pct = max(self.pct, pct)
            if msg and msg.strip():
                self._log.append(msg.strip())
            lo = msg.lower()
            if "match" in lo:
                for tok in msg.split():
                    try:
                        n = int(tok)
                        if n >= self.match_count:
                            self.match_count = n
                        break
                    except ValueError:
                        pass

    def snapshot(self):
        with self._lock:
            return (
                self.pct,
                self.match_count,
                list(self._log),
                time.monotonic() - self.start_time,
            )

def _build_live_renderable(state: _SearchState):
    def _render() -> Text:
        pct, matches, log_lines, elapsed = state.snapshot()

        out = Text()

        padded = log_lines + [""] * (state.LOG_LINES - len(log_lines))
        for line in padded:
            if line:
                out.append("  [", style="color(88)")
                out.append(" " + line + " ", style="color(167)")
                out.append("]\n", style="color(88)")
            else:
                out.append("\n")

        out.append("\n")
        out.append("  Working\n", style="bold white")

        out.append("  ")
        out.append_text(_render_bar(pct))

        pct_color = "bold green" if pct >= 100 else "bold white"
        out.append(f"   {pct:>3.0f}%", style=pct_color)
        out.append("  ·  ", style="muted")
        out.append(str(matches), style="bold green")
        out.append(" matches", style="muted")
        out.append("  ·  ", style="muted")
        mins, secs = divmod(int(elapsed), 60)
        out.append(f"{mins}:{secs:02d}", style="muted")
        out.append("\n")

        return out

    return _render

def _run_search(params: dict) -> dict:
    state  = _SearchState()
    result_box: list = []

    def _worker():
        def _cb(pct: int, msg: str) -> None:
            state.push(pct, msg)

        try:
            raw = eng.search_transcripts(json.dumps(params), progress_callback=_cb)
            parsed = json.loads(raw)
            if not isinstance(parsed, dict):
                raise ValueError("Engine response is not a JSON object")
            result_box.append(parsed)
        except Exception as exc:
            result_box.append({
                "status": "error",
                "error": f"Engine execution failed: {exc}",
                "match_count": 0,
                "results": [],
            })

    worker = threading.Thread(target=_worker, daemon=True)
    worker.start()

    renderable = _build_live_renderable(state)

    with Live(
        renderable(),
        console=console,
        refresh_per_second=15,
        transient=True,
    ) as live:
        while worker.is_alive():
            live.update(renderable())
            time.sleep(1 / 15)
        state.push(100, "")
        live.update(renderable())

    worker.join()
    return result_box[0] if result_box else {}

def _ok  (msg: str) -> None: console.print(f"  [bold green]>>[/bold green]  {msg}")
def _warn(msg: str) -> None: console.print(f"  [bold yellow]!![/bold yellow]  {msg}")
def _err (msg: str) -> None: console.print(f"  [bold red]xx[/bold red]  {msg}")
def _info(msg: str) -> None: console.print(f"  [grey46]--[/grey46]  [dim]{msg}[/dim]")

def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        prog="capscript",
        description="Search YouTube captions for keywords.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples
--------
  # Search a channel by @handle for the last 20 videos
  python cli.py --search-type channel --channel "@mkbhd" \\
                --keyword "sponsors" --max-results 20

  # Search specific video URLs / IDs
  python cli.py --search-type video \\
                --video-ids "https://youtu.be/dQw4w9WgXcQ,abc123XYZ00" \\
                --keyword "never gonna"

    # Force legacy substring behavior (matches within larger words)
    python cli.py --search-type video --video-ids abc123 \\
                                --keyword "how" --match-mode contains

  # Use a Webshare proxy and a cookies file
  python cli.py --search-type channel --channel "UCxxxxxx" --keyword "AI" \\
                --proxy-type webshare --proxy-username user --proxy-password pass \\
                --cookies cookies.txt

  # Persist proxy settings and run later without repeating them
  python cli.py --proxy-type generic --proxy-url "http://1.2.3.4:8080" --save-proxy
  python cli.py --search-type video --video-ids abc123 --keyword test

  # Use a custom output filename
  python cli.py --search-type video --video-ids abc123 --keyword test \\
                --output-file my_hits.txt
""",
    )

    tg = p.add_argument_group("Search target (one required)")
    tg.add_argument("--search-type", choices=["channel", "video"],
        help="'channel' to scan recent uploads; 'video' for explicit IDs/URLs.")
    tg.add_argument("--channel", metavar="CHANNEL", dest="channel_id",
        help="Channel ID (UCxxx…), @handle, or full channel URL.")
    tg.add_argument("--max-results", type=int, default=10, metavar="N",
        help="Max videos with captions to fetch from the channel (default: 10).")
    tg.add_argument("--video-ids", metavar="IDS_OR_FILE",
        help="Comma-separated video IDs or URLs, or a path to a file with one per line.")

    sg = p.add_argument_group("Search options")
    sg.add_argument("--keyword", metavar="WORD",
        help="Keyword or phrase to search for in captions.")
    sg.add_argument("--match-mode", metavar="MODE", default="smart",
        choices=["smart", "exact_phrase", "contains"],
        help="Search behavior: smart (default), exact_phrase, contains.")
    sg.add_argument("--language", metavar="LANG", default="en",
        help="BCP-47 language code for captions (default: en).")
    sg.add_argument("--output-dir", metavar="DIR", default="transcripts",
        help="Directory for result .txt files (default: transcripts).")
    sg.add_argument("--output-file", metavar="FILE",
        help="Optional output filename (example: results.txt).")

    pg = p.add_argument_group("Proxy settings")
    pg.add_argument("--proxy-type", choices=["none", "webshare", "generic"])
    pg.add_argument("--proxy-username", metavar="USER")
    pg.add_argument("--proxy-password", metavar="PASS")
    pg.add_argument("--proxy-url",      metavar="URL")
    pg.add_argument("--save-proxy", action="store_true",
        help="Persist proxy settings to preferences.ini for future runs.")
    pg.add_argument("--clear-proxy", action="store_true",
        help="Remove saved proxy settings and exit.")

    ag = p.add_argument_group("Authentication")
    ag.add_argument("--cookies", metavar="FILE",
        help="Path to a Netscape/Mozilla cookies.txt file.")
    ag.add_argument("--cookies-from-browser", metavar="BROWSER",
        help="Name of the browser to load cookies from (e.g., chrome, firefox, edge, vivaldi, brave).")

    # Hidden legacy flags accepted for backward compatibility.
    p.add_argument("--api-key", default="", help=argparse.SUPPRESS)
    p.add_argument("--save-api-key", action="store_true", help=argparse.SUPPRESS)
    p.add_argument("--validate-api-key", action="store_true", help=argparse.SUPPRESS)

    return p

def main() -> None:
    parser = build_parser()
    args   = parser.parse_args()

    console.print()

    if args.clear_proxy:
        eng.save_proxy_settings("none")
        _ok("Proxy settings cleared.")
        return

    if args.api_key:
        _warn("Legacy --api-key is deprecated and ignored in yt-dlp mode.")
    if args.save_api_key:
        _warn("Legacy --save-api-key is deprecated and ignored.")
    if args.validate_api_key:
        _warn("Legacy --validate-api-key is deprecated and ignored.")

    saved_proxy    = json.loads(eng.load_proxy_settings())
    proxy_type     = args.proxy_type     or saved_proxy.get("type",     "none")
    proxy_username = args.proxy_username or saved_proxy.get("username", "")
    proxy_password = args.proxy_password or saved_proxy.get("password", "")
    proxy_url      = args.proxy_url      or saved_proxy.get("url",      "")

    if proxy_type == "none":
        proxy_type = None

    if args.save_proxy:
        eng.save_proxy_settings(
            proxy_type or "none", proxy_username, proxy_password, proxy_url
        )
        _ok("Proxy settings saved.")
        if not args.search_type:
            return

    if not args.search_type:
        if args.save_api_key or args.validate_api_key:
            return
        parser.error("--search-type is required for a search.")
    if not args.keyword:
        parser.error("--keyword is required for a search.")
    if args.search_type == "channel" and not args.channel_id:
        parser.error("--channel is required when --search-type is 'channel'.")
    if args.search_type == "video" and not args.video_ids:
        parser.error("--video-ids is required when --search-type is 'video'.")
    if args.max_results <= 0:
        parser.error("--max-results must be a positive integer.")

    console.print(
        f"  [muted]keyword[/muted]  [color(196)]{args.keyword}[/color(196)]"
        f"   [muted]match[/muted] [dim]{args.match_mode}[/dim]"
        f"   [muted]mode[/muted] [color(196)]{args.search_type}[/color(196)]"
        f"   [muted]lang[/muted] [dim]{args.language}[/dim]"
        f"   [muted]proxy[/muted] [dim]{proxy_type or 'none'}[/dim]"
    )
    console.print()

    params = {
        "api_key":         "",
        "search_type":     args.search_type,
        "keyword":         args.keyword,
        "match_mode":      args.match_mode,
        "language":        args.language,
        "output_dir":      args.output_dir,
        "output_filename": args.output_file or None,
        "channel_id":      args.channel_id or "",
        "max_results":     args.max_results,
        "video_ids_input": args.video_ids or "",
        "cookies_file":    args.cookies or None,
        "cookies_from_browser": args.cookies_from_browser or None,
        "proxy_type":      proxy_type,
        "proxy_username":  proxy_username,
        "proxy_password":  proxy_password,
        "proxy_url":       proxy_url,
    }

    result = _run_search(params)

    status = str(result.get("status") or "ok").strip().lower()
    if status not in {"ok", "error", "cancelled"}:
        _warn(f"Unexpected engine status '{status}'. Treating as error.")
        status = "error"

    match_count = int(result.get("match_count", 0) or 0)
    output_file = result.get("output_file")
    error_msg = str(result.get("error") or "Search failed")

    console.print()
    if status == "error":
        _err(error_msg)
        console.print()
        sys.exit(1)

    if status == "cancelled":
        if match_count > 0:
            _warn(
                f"Search cancelled with [bold]{match_count}[/bold] partial "
                f"match{'es' if match_count != 1 else ''}."
            )
            if output_file:
                _info(f"partial results saved → [accent]{output_file}[/accent]")
        else:
            _warn("Search cancelled.")
        console.print()
        sys.exit(130)

    if match_count > 0:
        _ok(f"[bold]{match_count}[/bold] match{'es' if match_count != 1 else ''} found.")
        if output_file:
            _ok(f"saved → [accent]{output_file}[/accent]")
    else:
        _warn(f'No matches found for [bold]"{args.keyword}"[/bold].')

    console.print()

if __name__ == "__main__":
    main()