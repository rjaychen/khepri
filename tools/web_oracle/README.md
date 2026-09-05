# Khepri Web Oracle

A lightweight browser streaming server and bidirectional input bridge for testing and interacting with the Khepri Vulkan graphics engine.

## Overview

The Web Oracle captures the Khepri Engine's OS window client rectangle and streams high-framerate JPEG video frames to an HTML5 Canvas over WebSockets. User and subagent interactions in the browser (such as clicks, drags, mouse wheel, and keyboard inputs) are converted in real time into native Win32 input events and injected directly into Khepri Engine.

### Features
- **Zero-Engine Overhead**: Requires no modifications or custom socket/IPC code in the C++ Vulkan engine.
- **Isolated Virtual Desktop Mode**: Runs Khepri on a dedicated Win32 hidden desktop (`winsta0\KhepriVirtualDesktop`), eliminating window occlusion and preventing synthetic mouse/keyboard events from moving your physical mouse cursor or stealing focus.
- **Sub-millisecond Latency**: Live WebSocket streaming with immediate event response.
- **Pure Interactive Canvas**: Responsive, fullscreen HTML5 canvas with normalized coordinate mapping.
- **Comprehensive Input Forwarding**: Left/Right/Middle clicks, click-and-drag (for camera orbiting and UI sliders), mouse wheel zooming, and full keyboard navigation.
- **Auto-Discovery & Auto-Launch**: Finds existing Khepri windows automatically or launches `build/Debug/KhepriEngine.exe` / `build/Release/KhepriEngine.exe` if not currently running.

---

## Quickstart

### 1. Isolated Virtual Desktop Mode (Recommended for Testing)
```powershell
# Launches Khepri in background on its own isolated virtual desktop
.\tools\web_oracle\run_oracle.ps1 -Isolated
```
Or via Python:
```powershell
py tools/web_oracle/server.py --port 8080 --virtual-desktop
```

### 2. Standard Foreground Desktop Mode
```powershell
.\tools\web_oracle\run_oracle.ps1
```

### 3. Open in Browser
Navigate to:
```
http://localhost:8080
```

---

## CLI Options

| Option | Type | Default | Description |
|---|---|---|---|
| `--host` | `str` | `0.0.0.0` | Host IP address to bind |
| `--port` | `int` | `8080` | Port to listen on |
| `--fps` | `int` | `30` | Target streaming framerate |
| `--quality` | `int` | `75` | JPEG quality (1-100) |
| `--title` | `str` | `Khepri Engine` | Substring to discover in window title |
| `--no-launch` | `flag` | `False` | Disable automatic engine launching |
| `--exe` | `str` | `None` | Explicit path to `KhepriEngine.exe` |
| `--virtual-desktop` | `str` (optional) | `None` (or `KhepriVirtualDesktop`) | Run Khepri on a dedicated Win32 Virtual Desktop |

---

## Testing with Antigravity Browser Subagent

When invoking the browser subagent in Antigravity to verify Khepri UI components:
1. Start the oracle in isolated mode: `.\tools\web_oracle\run_oracle.ps1 -Isolated -Port 8080`
2. Direct the browser subagent to `http://localhost:8080`.
3. The subagent can click on ImGui panels (Scene Hierarchy, Asset Manager, Animation Timeline, Inspector), drag gizmos, orbit the 3D viewport with right-click drag, and verify UI rendering without moving your physical cursor.
