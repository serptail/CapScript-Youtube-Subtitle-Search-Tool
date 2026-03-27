<p align="center">
  <a href="https://ko-fi.com/serptail">
    <img src="https://ko-fi.com/img/githubbutton_sm.svg" alt="Support me on Ko-fi" />
  </a>
</p>

<p align="center">
[![Release](https://img.shields.io/badge/Release-v2.0-red.svg)](https://github.com/serptail/CapScript-Youtube-Subtitle-Search-Tool/releases)
![Platform](https://img.shields.io/badge/Platform-Windows-lightgrey.svg)
[![License](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)
</p>

<p align="center">
<img width="1024" height="524" alt="capscript-png-noborder" src="https://github.com/user-attachments/assets/68c203e3-846c-41bc-bac7-443044638413" />
<br>
  <em><strong>CapScript Pro, GUI Based YouTube Transcript Search Tool</strong></em>
</p>

# CapScript Pro

CapScript Pro is a Windows desktop application for searching YouTube captions at scale, then turning those findings into usable media outputs.

This repository includes:
- A modern Qt/C++ desktop app.
- An embedded Python engine for YouTube API, transcript fetching, and search logic.
- A standalone Python CLI for headless/automation workflows.

## Table of Contents

- [Overview](#overview)
- [Desktop App Features](#desktop-app-features)
- [How the Embedded Python Engine Works](#how-the-embedded-python-engine-works)
- [Tech Stack and Dependencies](#tech-stack-and-dependencies)
- [Project Structure](#project-structure)
- [Desktop App Setup](#desktop-app-setup)
- [Build and Run (Qt App)](#build-and-run-qt-app)
- [CLI Guide](#cli-guide)
- [CLI Setup and Requirements](#cli-setup-and-requirements)
- [CLI Usage](#cli-usage)
- [CLI Examples](#cli-examples)
- [Troubleshooting](#troubleshooting)

## Overview

CapScript Pro solves a practical workflow:
1. Find subtitle matches for a keyword across videos.
2. Review exact timestamps inside the built-in viewer.
3. Extract clips from the matched moments.
4. Render clips into a final output video.
5. Build reusable video ID lists for future runs.

The UI is native Qt (C++), while core YouTube logic is powered by Python.


## Desktop App Features

- Search Page: Handles API configuration and flexible search queries for videos or channels. It includes advanced network settings (proxy, cookies), language selection, and encrypted credential persistence.
- Viewer Page: Displays structured transcripts with clickable timestamps and supports in-app video playback. It also allows users to export or import transcript files.
- Clip Downloader: Parses transcripts to identify specific clips for download. It supports various formats (mp4, mkv, webm, mp3), quality controls, and uses worker threads for efficient background processing.
- Renderer Page: Merges downloaded clips into a single final video using FFmpeg. It auto-detects formats and utilizes stream copying for fast, lossless concatenation.
- List Creator: Fetches channel videos based on date ranges or keywords. It provides a selectable list with thumbnail previews and options to export video IDs.
- System & Support: Includes an integrated update checker that stages updates automatically.

## How the Embedded Python Engine Works

The desktop app starts a bundled Python runtime and imports capscript_engine.py through a C++ bridge.

High-level flow:
1. The app resolves Python runtime paths near the executable.
2. PythonBridge initializes Python with controlled module search paths.
3. C++ invokes Python functions for:
   - API key validation and storage.
   - Proxy settings storage/loading.
   - Channel resolution and video lookups.
   - Transcript search execution.
4. Search progress is sent back into C++ via callback trampolines and displayed in UI/CLI.

This split keeps the UI native and fast while preserving Python ecosystem advantages for YouTube-related operations.

## Tech Stack and Dependencies

### Desktop Application

- C++17
- Qt 6 modules:
  - Core, Gui, Widgets, Network, Concurrent, Svg
  - Optional: Multimedia, MultimediaWidgets
  - Optional: Quick, Qml, QuickWidgets, WebView, WebViewQuick
- Python 3 interpreter + development libs for embedding
- Windows WebView2 runtime/SDK (optional but recommended for in-app player)

### Python Engine and CLI

From python/requirements.txt:
- youtube-transcript-api
- google-api-python-client
- google-auth
- google-auth-httplib2
- google-api-core
- httplib2
- requests
- cryptography
- rich

### External Media Tools

Required for clip download/render features:
- yt-dlp
- ffmpeg
- ffprobe (normally included with ffmpeg)

The app looks for tools in:
- app/bin/
- app root
- system PATH

### Optional Service Component

Cloudflare Worker (TypeScript) for feedback delivery and basic rate limiting.

## Project Structure

- src/: Qt C++ application and workers
- python/: capscript_engine.py + cli.py + Python requirements
- scripts/: utility scripts (including embedded runtime bundling)
- qml/: QML resources for web player integrations
- assets/: app fonts, icons, resources
- cloudflare-worker/: optional feedback backend
- docs/: web documentation assets
- third_party/webview2/: WebView2 SDK assets

## Desktop App Setup

### Prerequisites

- Windows 10/11
- CMake 3.21+
- Qt 6 (matching your compiler toolchain)
- Python 3 with headers/libs discoverable by CMake
- Optional but recommended: WebView2 runtime installed
- Optional for clip/render workflow: yt-dlp and ffmpeg available

### Optional: Bundle Python Runtime for Distribution

Use the provided script to package embeddable Python + pip dependencies into a target python folder:

```powershell
python scripts/bundle_python.py 3.11.9 .\python .\python\requirements.txt
```

## Build and Run (Qt App)

Example CMake flow (Visual Studio generator):

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

Run the desktop app from build output after ensuring python/ runtime assets are present near the executable.

## CLI Guide

The repository includes a full command-line interface in python/cli.py for non-GUI usage.

Use CLI when you need:
- Automation (batch jobs, scripting, CI-like tasks).
- Remote/headless execution.
- Fast one-off searches without opening the desktop app.

## CLI Setup and Requirements

### 1. Python Version

Recommended: Python 3.10+.

### 2. Install Dependencies

```powershell
cd python
python -m pip install -r requirements.txt
```

### 3. API Key

You need a YouTube Data API v3 key.

You can provide it every run:

```powershell
python cli.py --api-key YOUR_KEY ...
```

Or save once (encrypted in preferences.ini):

```powershell
python cli.py --api-key YOUR_KEY --save-api-key --search-type video --video-ids dQw4w9WgXcQ --keyword test
```

### 4. Optional Network Setup

- Cookies file (Netscape/Mozilla format): --cookies path\to\cookies.txt
- Proxy modes:
  - --proxy-type webshare with username/password
  - --proxy-type generic with proxy URL
- Persist proxy settings with --save-proxy
- Clear saved proxy with --clear-proxy

## CLI Usage

### Main Search Command

```powershell
python cli.py --search-type channel --channel "@mkbhd" --keyword "sponsors" --max-results 20
```

### Required Inputs

- --search-type channel or video
- --keyword "your search term"
- For channel mode: --channel
- For video mode: --video-ids

### Useful Options

- --language en
- --output-dir transcripts
- --validate-api-key
- --save-api-key
- --cookies FILE
- --proxy-type, --proxy-username, --proxy-password, --proxy-url
- --save-proxy, --clear-proxy

## CLI Examples

### 1. Search a channel by handle

```powershell
python cli.py --search-type channel --channel "@mkbhd" --keyword "sponsors" --max-results 20
```

### 2. Search explicit video URLs/IDs

```powershell
python cli.py --search-type video --video-ids "https://youtu.be/dQw4w9WgXcQ,abc123XYZ00" --keyword "never gonna"
```

### 3. Save API key for future runs

```powershell
python cli.py --api-key YOUR_KEY --save-api-key --search-type video --video-ids dQw4w9WgXcQ --keyword test
```

### 4. Use Webshare proxy + cookies

```powershell
python cli.py --search-type channel --channel "UCxxxxxx" --keyword "AI" --proxy-type webshare --proxy-username USER --proxy-password PASS --cookies cookies.txt
```

### 5. Save generic proxy once

```powershell
python cli.py --proxy-type generic --proxy-url "http://1.2.3.4:8080" --save-proxy
python cli.py --search-type video --video-ids abc123 --keyword test
```

## Troubleshooting

- Python engine fails to initialize in app:
  - Ensure python/ exists near the executable and contains runtime artifacts (Lib, python3xx zip, required modules).
- No transcript results:
  - Try cookies.txt and/or proxy options.
  - Verify language code availability for each video.
- Clip download/render not working:
  - Confirm yt-dlp and ffmpeg are installed and discoverable.
  - Check app logs for tool path and process errors.
- API errors or quota exceeded:
  - Validate key with --validate-api-key.
  - Check YouTube Data API quota status in Google Cloud.







