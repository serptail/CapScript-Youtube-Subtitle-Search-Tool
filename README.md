<div align="center">

[![Release](https://img.shields.io/badge/Release-v2.6.0-red.svg)](https://github.com/serptail/CapScript-Youtube-Subtitle-Search-Tool/releases) ![Platform](https://img.shields.io/badge/Platform-Windows-blue.svg) [![License](https://img.shields.io/badge/License-MIT%20%2B%20Commons%20Clause-white.svg)](LICENSE)

</div>


<p align="center">
  <img width="300"alt="app_icon_filled" src="https://github.com/user-attachments/assets/88a7e846-a3d9-4f6f-8770-dd7b27a57d7e" />
  <br>
  <em><strong>A GUI-based YouTube transcript search tool for Windows</strong></em>
</p>

## Table of Contents

- [Overview](#overview)
- [Features](#features)
- [Architecture](#architecture)
- [Tech Stack](#tech-stack)
- [Project Structure](#project-structure)
- [Installation](#installation)
- [Building from Source](#building-from-source)
- [CLI Reference](#cli-reference)
- [Troubleshooting](#troubleshooting)
- [Contributing](#contributing)
- [License](#license)

## Overview

CapScript Pro searches YouTube captions at scale and turns the results into usable media — clips, rendered videos, and reusable video-ID lists. It ships as a native Qt/C++ desktop app for Windows, backed by an embedded Python engine, plus a standalone CLI for headless or automated workflows.

**Typical workflow:**

1. Search for a keyword across a channel or a list of videos.
2. Review exact timestamp matches in the built-in transcript viewer.
3. Extract clips at the matched moments.
4. Render clips into a single output video via FFmpeg.
5. Save reusable video-ID lists for future searches.

> **No YouTube Data API.** As of v2.5, CapScript Pro runs entirely on [yt-dlp](https://github.com/yt-dlp/yt-dlp) — no API key, no quota limits, no credential management.

## Features

| Page | What it does |
|---|---|
| **Search** | Query by video or channel, with proxy support, cookie-based auth, and language selection |
| **Viewer** | Browse transcripts with clickable timestamps and in-app playback; import/export transcripts |
| **Clip Downloader** | Pull clips from matched timestamps in mp4, mkv, webm, or mp3, with quality controls |
| **Renderer** | Merge clips into a final video via FFmpeg, with control over format, resolution, frame rate, and CRF |
| **List Creator** | Build video-ID lists from a channel by date range or keyword, with thumbnail previews |
| **Updater** | Built-in update checker that stages new releases automatically |

## Architecture

The UI is native Qt/C++; YouTube-facing logic runs in an embedded Python runtime.

1. The app resolves Python runtime paths relative to the executable.
2. `PythonBridge` initializes an embedded interpreter with scoped module search paths.
3. C++ calls into Python for channel resolution, transcript search, and proxy/credential storage.
4. Progress streams back to C++ via callback trampolines and updates the UI (or CLI output) live.

This keeps the interface fast and native while reusing the Python/yt-dlp ecosystem for the parts that need it.

## Tech Stack

**Desktop app**
- C++17, Qt 6 (Core, Gui, Widgets, Network, Concurrent, Svg)
- Optional: Multimedia/MultimediaWidgets, Quick/Qml/QuickWidgets/WebView/WebViewQuick
- Embedded Python 3 interpreter
- WebView2 runtime (optional, enables the in-app player)

**Python engine / CLI** — see `python/requirements.txt`
- yt-dlp, cryptography, rich

**External tools** (required for clip/render features) — resolved from `app/bin/`, the app root, or system `PATH`
- yt-dlp, ffmpeg, ffprobe

**Optional**
- A Cloudflare Worker (TypeScript) handles feedback delivery and rate limiting.

## Project Structure

```
.github/workflows/                 CI workflows
assets/
├── fonts/                         Bundled fonts (+ licenses)
├── icons/                         App icons
└── qml/                           QML resources for web player integration
cloudflare-worker/
└── capscript-feedback-worker/     Optional feedback backend (TypeScript)
docs/                              Web documentation assets
python/                            capscript_engine.py, cli.py, requirements.txt
scripts/                           Build utilities, incl. the embedded runtime bundler
src/
├── app/                           Application entry point
├── core/                          Python bridge, settings, updater, URL handling
├── ui/
│   ├── pages/                     Search, Viewer, Clip Downloader, Renderer, List Creator, About
│   ├── styles/                    Theming (ThemeManager)
│   └── widgets/                   Reusable UI widgets
├── updater/                       Standalone updater executable
└── workers/                       Background threads for search, clip, and render jobs
third_party/
├── phantomstyle/                  Vendored Qt style (src/phantom, src/styleplugin)
└── webview2/sdk/                  WebView2 SDK headers and native libs
CMakeLists.txt
CapScriptPro.rc
CapScriptUpdater.rc
LICENSE
```

## Installation

CapScript Pro can be used two ways: the **GUI app** (Windows only, no setup) or the **CLI** (cross-platform-friendly, requires Python).

### Option 1: GUI (recommended for most users)

1. Download the latest Windows build from [Releases](https://github.com/serptail/CapScript-Youtube-Subtitle-Search-Tool/releases) (currently **v2.6.0**).
2. Extract the archive and run `CapScriptPro.exe`.
3. No separate Python or Qt install is needed — the release ships with a bundled Python runtime and all required DLLs.

> **Optional - Note:** CapScript Pro bundles the WebView2 *loader* only — the actual WebView2 Runtime (Microsoft's embedded browser engine) is not shipped with the app. It comes preinstalled on virtually all up-to-date Windows 10/11 systems alongside Edge. If it's missing, the in-app player won't load; install it from [Microsoft](https://developer.microsoft.com/microsoft-edge/webview2/) (Evergreen Bootstrapper, a ~2 MB installer).

### Option 2: CLI (for automation / headless use)

```powershell
git clone https://github.com/serptail/CapScript-Youtube-Subtitle-Search-Tool.git
cd CapScript-Youtube-Subtitle-Search-Tool/python
python -m pip install -r requirements.txt
```

Then run searches directly:

```powershell
python cli.py --search-type channel --channel "@mkbhd" --keyword "sponsors" --max-results 20
```

See [CLI Reference](#cli-reference) below for full usage, options, and examples. `ffmpeg` and `yt-dlp` should be on your `PATH` if you plan to use clip/render features from the CLI as well.

## Building from Source

### Prerequisites

- Windows 10/11
- CMake 3.21+
- Qt 6 (matching your compiler toolchain)
- Python 3 with headers/libs discoverable by CMake
- WebView2 runtime (optional, recommended)
- yt-dlp and ffmpeg on `PATH` (optional, needed for clip/render features)

### Build

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

The build output includes the app executable alongside a bundled `python/` runtime — no additional setup needed to run it.

### Bundling Python for distribution

To package an embeddable Python runtime with dependencies manually:

```powershell
python scripts/bundle_python.py 3.11.9 .\python .\python\requirements.txt
```

## CLI Reference

The Python CLI (`python/cli.py`) covers automation, headless environments, and quick one-off searches without launching the GUI.

### Setup

```powershell
cd python
python -m pip install -r requirements.txt
```

Recommended: Python 3.10+.

### Authentication

No API key is needed — everything runs through yt-dlp. For age-restricted or private content, pass cookies directly from your browser:

```powershell
--cookies-from-browser chrome|firefox|edge|brave
```

Legacy API-key flags are still accepted as no-ops, for compatibility with older scripts.

### Usage

```powershell
python cli.py --search-type channel --channel "@mkbhd" --keyword "sponsors" --max-results 20
```

**Required**
- `--search-type` — `channel` or `video`
- `--keyword` — search term
- `--channel` (channel mode) or `--video-ids` (video mode)

**Common options**
- `--language en`
- `--output-dir transcripts`
- `--cookies FILE` / `--cookies-from-browser chrome|firefox|edge|brave`
- `--proxy-type`, `--proxy-username`, `--proxy-password`, `--proxy-url`
- `--save-proxy`, `--clear-proxy`

### Examples

```powershell
# Search a channel by handle
python cli.py --search-type channel --channel "@mkbhd" --keyword "sponsors" --max-results 20

# Search explicit video URLs/IDs
python cli.py --search-type video --video-ids "https://youtu.be/dQw4w9WgXcQ,abc123XYZ00" --keyword "never gonna"

# Use browser cookies
python cli.py --search-type channel --channel "@mkbhd" --keyword "AI" --cookies-from-browser chrome

# Webshare proxy + cookies file
python cli.py --search-type channel --channel "UCxxxxxx" --keyword "AI" --proxy-type webshare --proxy-username USER --proxy-password PASS --cookies cookies.txt

# Save a generic proxy once, reuse on later runs
python cli.py --proxy-type generic --proxy-url "http://1.2.3.4:8080" --save-proxy
python cli.py --search-type video --video-ids abc123 --keyword test
```

## Troubleshooting

| Symptom | Fix |
|---|---|
| Python engine fails to initialize | Confirm `python/` exists next to the executable with `Lib`, the Python zip, and required modules |
| No transcript results | Try `--cookies-from-browser` or a `cookies.txt`, and confirm the language code exists for that video |
| Clip download/render fails | Verify yt-dlp and ffmpeg are installed and discoverable; check app logs for tool-path errors |
| Rate limiting / access errors | Pass browser cookies via `--cookies-from-browser`, reduce concurrent activity, and retry |

## Contributing

Found a bug or have a feature idea? [Open an issue](https://github.com/serptail/CapScript-Youtube-Subtitle-Search-Tool/issues) — bug reports and suggestions are both welcome.

## License

MIT License + Commons Clause v1.0.

Free to use, modify, and distribute — but not to sell or offer as a paid/SaaS service. See [LICENSE](LICENSE) for the full text.

---

## Screenshots

<p align="center">
  <img width="100%" alt="List Creator tab" src="https://github.com/user-attachments/assets/94bd79fd-c2aa-457a-b8e9-6428129c1a63" />
  <br><em>List Creator</em>
</p>
<p align="center">
  <img width="100%" alt="Downloader tab" src="https://github.com/user-attachments/assets/ddc686f5-9d85-459d-8927-e498fe29e87a" />
  <br><em>Clip Downloader</em>
</p>
