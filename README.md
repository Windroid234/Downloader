# Downloader

A system tray application for downloading videos from YouTube and Instagram.
*Note: This was originally a private project.*

## Features
- System tray UI
- Live download progress
- Automatic clipboard copying
- Configuration and history persistence

## Credits
- yt-dlp

## Building
1. Clone this repository.
2. Run `cmake -B build` and `cmake --build build --config Release`.
3. Download `yt-dlp.exe` and place it in the same directory as the built executable.

## Installing
1. Compile `installer.iss` using Inno Setup Compiler.
2. Run the generated setup executable.
