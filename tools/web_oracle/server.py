#!/usr/bin/env python3
"""
Khepri Web Oracle Streaming & Input Bridge Server.

Streams the Khepri Engine application viewport to a web browser endpoint over WebSockets
and translates browser mouse, drag, scroll, and keyboard events back into native Win32 inputs.

Supports both standard desktop capture and isolated Win32 Virtual Desktop mode.
"""

import argparse
import asyncio
import atexit
import ctypes
import ctypes.wintypes
import io
import json
import mimetypes
import os
import subprocess
import sys
import time
from pathlib import Path
from typing import Optional, Set, Tuple

try:
    sys.stdout.reconfigure(line_buffering=True)
except Exception:
    pass

import mss
from PIL import Image
import win32gui
import win32ui
import win32con
import win32process
import websockets
from websockets.asyncio.server import serve, ServerConnection
from websockets.http11 import Request, Response
from websockets.datastructures import Headers

# ---------------------------------------------------------------------------
# Win32 Initialization & High DPI Setup
# ---------------------------------------------------------------------------
user32 = ctypes.windll.user32
kernel32 = ctypes.windll.kernel32
gdi32 = ctypes.windll.gdi32

try:
    # Set Per-Monitor DPI Awareness (V2)
    ctypes.windll.shcore.SetProcessDpiAwareness(2)
except Exception:
    try:
        user32.SetProcessDPIAware()
    except Exception:
        pass


def setup_thread_desktop(hdesk: Optional[int] = None):
    """Ensures the calling thread is attached to the desired input desktop."""
    if hdesk:
        user32.SetThreadDesktop(hdesk)
    else:
        hdesk_input = user32.OpenInputDesktop(0, False, 0x01FF)
        if hdesk_input:
            user32.SetThreadDesktop(hdesk_input)


setup_thread_desktop()

# ---------------------------------------------------------------------------
# Win32 Constants
# ---------------------------------------------------------------------------
SW_RESTORE = 9
SW_SHOWNORMAL = 1

MOUSEEVENTF_MOVE = 0x0001
MOUSEEVENTF_LEFTDOWN = 0x0002
MOUSEEVENTF_LEFTUP = 0x0004
MOUSEEVENTF_RIGHTDOWN = 0x0008
MOUSEEVENTF_RIGHTUP = 0x0010
MOUSEEVENTF_MIDDLEDOWN = 0x0020
MOUSEEVENTF_MIDDLEUP = 0x0040
MOUSEEVENTF_WHEEL = 0x0800
MOUSEEVENTF_ABSOLUTE = 0x8000

KEYEVENTF_KEYUP = 0x0002
KEYEVENTF_EXTENDEDKEY = 0x0001

# Virtual Key Mapping Table (DOM event.code & event.key -> Win32 VK)
VK_MAP = {
    # Letters
    "KeyA": 0x41, "KeyB": 0x42, "KeyC": 0x43, "KeyD": 0x44, "KeyE": 0x45,
    "KeyF": 0x46, "KeyG": 0x47, "KeyH": 0x48, "KeyI": 0x49, "KeyJ": 0x4A,
    "KeyK": 0x4B, "KeyL": 0x4C, "KeyM": 0x4D, "KeyN": 0x4E, "KeyO": 0x4F,
    "KeyP": 0x50, "KeyQ": 0x51, "KeyR": 0x52, "KeyS": 0x53, "KeyT": 0x54,
    "KeyU": 0x55, "KeyV": 0x56, "KeyW": 0x57, "KeyX": 0x58, "KeyY": 0x59, "KeyZ": 0x5A,
    # Digits
    "Digit0": 0x30, "Digit1": 0x31, "Digit2": 0x32, "Digit3": 0x33, "Digit4": 0x34,
    "Digit5": 0x35, "Digit6": 0x36, "Digit7": 0x37, "Digit8": 0x38, "Digit9": 0x39,
    # Numpad
    "Numpad0": 0x60, "Numpad1": 0x61, "Numpad2": 0x62, "Numpad3": 0x63, "Numpad4": 0x64,
    "Numpad5": 0x65, "Numpad6": 0x66, "Numpad7": 0x67, "Numpad8": 0x68, "Numpad9": 0x69,
    "NumpadMultiply": 0x6A, "NumpadAdd": 0x6B, "NumpadSeparator": 0x6C,
    "NumpadSubtract": 0x6D, "NumpadDecimal": 0x6E, "NumpadDivide": 0x6F,
    "NumpadEnter": 0x0D, "NumpadEqual": 0x0C,
    # Navigation & Control
    "Space": 0x20, "Enter": 0x0D, "Escape": 0x1B, "Backspace": 0x08, "Tab": 0x09,
    "Delete": 0x2E, "Insert": 0x2D, "Home": 0x24, "End": 0x23, "PageUp": 0x21, "PageDown": 0x22,
    "ArrowLeft": 0x25, "ArrowUp": 0x26, "ArrowRight": 0x27, "ArrowDown": 0x28,
    "PrintScreen": 0x2C, "Pause": 0x13, "ScrollLock": 0x91, "CapsLock": 0x14, "NumLock": 0x90,
    # Modifiers
    "ShiftLeft": 0x10, "ShiftRight": 0x10,
    "ControlLeft": 0x11, "ControlRight": 0x11,
    "AltLeft": 0x12, "AltRight": 0x12,
    "MetaLeft": 0x5B, "MetaRight": 0x5C,
    # Function Keys
    "F1": 0x70, "F2": 0x71, "F3": 0x72, "F4": 0x73, "F5": 0x74, "F6": 0x75,
    "F7": 0x76, "F8": 0x77, "F9": 0x78, "F10": 0x79, "F11": 0x7A, "F12": 0x7B,
    "F13": 0x7C, "F14": 0x7D, "F15": 0x7E, "F16": 0x7F, "F17": 0x80, "F18": 0x81,
    "F19": 0x82, "F20": 0x83, "F21": 0x84, "F22": 0x85, "F23": 0x86, "F24": 0x87,
}

# ImGuiKey enum mapping from imgui.h (512 - 655)
IMGUI_KEY_MAP = {
    "Tab": 512, "ArrowLeft": 513, "ArrowRight": 514, "ArrowUp": 515, "ArrowDown": 516,
    "PageUp": 517, "PageDown": 518, "Home": 519, "End": 520, "Insert": 521, "Delete": 522,
    "Backspace": 523, "Space": 524, "Enter": 525, "Escape": 526,
    "ControlLeft": 527, "ShiftLeft": 528, "AltLeft": 529, "MetaLeft": 530,
    "ControlRight": 531, "ShiftRight": 532, "AltRight": 533, "MetaRight": 534,
    "Digit0": 542, "Digit1": 543, "Digit2": 544, "Digit3": 545, "Digit4": 546,
    "Digit5": 547, "Digit6": 548, "Digit7": 549, "Digit8": 550, "Digit9": 551,
    "KeyA": 552, "KeyB": 553, "KeyC": 554, "KeyD": 555, "KeyE": 556, "KeyF": 557,
    "KeyG": 558, "KeyH": 559, "KeyI": 560, "KeyJ": 561, "KeyK": 562, "KeyL": 563,
    "KeyM": 564, "KeyN": 565, "KeyO": 566, "KeyP": 567, "KeyQ": 568, "KeyR": 569,
    "KeyS": 570, "KeyT": 571, "KeyU": 572, "KeyV": 573, "KeyW": 574, "KeyX": 575,
    "KeyY": 576, "KeyZ": 577,
    "F1": 578, "F2": 579, "F3": 580, "F4": 581, "F5": 582, "F6": 583,
    "F7": 584, "F8": 585, "F9": 586, "F10": 587, "F11": 588, "F12": 589,
    "Apostrophe": 590, "Comma": 591, "Minus": 592, "Period": 593, "Slash": 594,
    "Semicolon": 595, "Equal": 596, "BracketLeft": 597, "Backslash": 598,
    "BracketRight": 599, "Backquote": 600,
}

# ---------------------------------------------------------------------------
# Oracle Direct IPC Pipe Client
# ---------------------------------------------------------------------------
class OraclePipeClient:
    def __init__(self, pipe_name: str = r"\\.\pipe\khepri_oracle_input"):
        self.pipe_name = pipe_name
        self.handle = None

    def connect(self) -> bool:
        if self.handle:
            return True
        try:
            self.handle = kernel32.CreateFileW(
                self.pipe_name,
                0x40000000,  # GENERIC_WRITE
                0,
                None,
                3,  # OPEN_EXISTING
                0,
                None
            )
            if self.handle == -1 or not self.handle:
                self.handle = None
                return False
            return True
        except Exception:
            self.handle = None
            return False

    def send_event(self, event_type: int, button: int, key: int, x: float, y: float, wheel: float) -> bool:
        if not self.connect():
            return False
        import struct
        packet = struct.pack('<BBHfff', event_type, button, key, float(x), float(y), float(wheel))
        written = ctypes.wintypes.DWORD(0)
        res = kernel32.WriteFile(self.handle, packet, len(packet), ctypes.byref(written), None)
        if not res:
            try:
                kernel32.CloseHandle(self.handle)
            except Exception:
                pass
            self.handle = None
            return False
        return True

    def close(self):
        if self.handle:
            try:
                kernel32.CloseHandle(self.handle)
            except Exception:
                pass
            self.handle = None

# ---------------------------------------------------------------------------
# Window & Desktop Management Helper
# ---------------------------------------------------------------------------
class KhepriWindowManager:
    def __init__(self, title_filter: str = "Khepri Engine", auto_launch: bool = True,
                 custom_exe: Optional[str] = None, virtual_desktop: Optional[str] = None):
        self.title_filter = title_filter
        self.auto_launch = auto_launch
        self.custom_exe = custom_exe
        self.virtual_desktop = virtual_desktop
        self.hdesk: Optional[int] = None
        self.hwnd: Optional[int] = None
        self.proc: Optional[subprocess.Popen] = None
        self.repo_root = Path(__file__).resolve().parent.parent.parent

        if self.virtual_desktop:
            self._init_virtual_desktop()

    def _init_virtual_desktop(self):
        """Creates or opens a named Win32 virtual desktop."""
        print(f"[KhepriOracle] Initializing Virtual Desktop: '{self.virtual_desktop}'...")
        # 0x01FF = GENERIC_ALL for Desktops
        self.hdesk = user32.CreateDesktopW(self.virtual_desktop, None, None, 0, 0x01FF, None)
        if not self.hdesk:
            self.hdesk = user32.OpenDesktopW(self.virtual_desktop, 0, False, 0x01FF)

        if self.hdesk:
            user32.SetThreadDesktop(self.hdesk)
            print(f"[KhepriOracle] Virtual Desktop created/opened (handle: {self.hdesk})")
        else:
            print(f"[KhepriOracle] WARNING: Failed to create/open virtual desktop '{self.virtual_desktop}'. Falling back to default desktop.")

    def find_window(self) -> Optional[int]:
        setup_thread_desktop(self.hdesk)
        found_hwnd = None

        class THREADENTRY32(ctypes.Structure):
            _fields_ = [
                ("dwSize", ctypes.wintypes.DWORD),
                ("cntUsage", ctypes.wintypes.DWORD),
                ("th32ThreadID", ctypes.wintypes.DWORD),
                ("th32OwnerProcessID", ctypes.wintypes.DWORD),
                ("tpBasePri", ctypes.wintypes.LONG),
                ("tpDeltaPri", ctypes.wintypes.LONG),
                ("dwFlags", ctypes.wintypes.DWORD),
            ]

        # 1. Gather target PIDs
        target_pids = set()
        if self.proc and self.proc.poll() is None:
            target_pids.add(self.proc.pid)

        # Snapshot processes to find all running KhepriEngine instances
        h_proc_snap = kernel32.CreateToolhelp32Snapshot(0x00000002, 0)  # TH32CS_SNAPPROCESS
        if h_proc_snap and h_proc_snap != -1:
            class PROCESSENTRY32(ctypes.Structure):
                _fields_ = [
                    ("dwSize", ctypes.wintypes.DWORD),
                    ("cntUsage", ctypes.wintypes.DWORD),
                    ("th32ProcessID", ctypes.wintypes.DWORD),
                    ("th32DefaultHeapID", ctypes.c_void_p),
                    ("th32ModuleID", ctypes.wintypes.DWORD),
                    ("cntThreads", ctypes.wintypes.DWORD),
                    ("th32ParentProcessID", ctypes.wintypes.DWORD),
                    ("pcPriClassBase", ctypes.wintypes.LONG),
                    ("dwFlags", ctypes.wintypes.DWORD),
                    ("szExeFile", ctypes.c_wchar * 260),
                ]
            pe = PROCESSENTRY32()
            pe.dwSize = ctypes.sizeof(PROCESSENTRY32)
            if kernel32.Process32FirstW(h_proc_snap, ctypes.byref(pe)):
                while True:
                    if "khepriengine" in pe.szExeFile.lower():
                        target_pids.add(pe.th32ProcessID)
                    if not kernel32.Process32NextW(h_proc_snap, ctypes.byref(pe)):
                        break
            kernel32.CloseHandle(h_proc_snap)

        if target_pids:
            h_thread_snap = kernel32.CreateToolhelp32Snapshot(0x00000004, 0)  # TH32CS_SNAPTHREAD
            if h_thread_snap and h_thread_snap != -1:
                te = THREADENTRY32()
                te.dwSize = ctypes.sizeof(THREADENTRY32)
                tids = []
                if kernel32.Thread32First(h_thread_snap, ctypes.byref(te)):
                    while True:
                        if te.th32OwnerProcessID in target_pids:
                            tids.append(te.th32ThreadID)
                        if not kernel32.Thread32Next(h_thread_snap, ctypes.byref(te)):
                            break
                kernel32.CloseHandle(h_thread_snap)

                WNDENUMPROC = ctypes.WINFUNCTYPE(ctypes.wintypes.BOOL, ctypes.wintypes.HWND, ctypes.wintypes.LPARAM)

                def thread_enum(h, _):
                    nonlocal found_hwnd
                    title_len = user32.GetWindowTextLengthW(h)
                    if title_len > 0:
                        buff = ctypes.create_unicode_buffer(title_len + 1)
                        user32.GetWindowTextW(h, buff, title_len + 1)
                        title = buff.value
                        if self.title_filter.lower() in title.lower():
                            found_hwnd = h
                            return False
                    return True

                cb = WNDENUMPROC(thread_enum)
                for tid in tids:
                    user32.EnumThreadWindows(tid, cb, 0)
                    if found_hwnd:
                        break

        # Fallback to standard EnumWindows
        if not found_hwnd:
            def enum_cb(hwnd, _):
                nonlocal found_hwnd
                title = win32gui.GetWindowText(hwnd)
                if not title:
                    return True
                lower_title = title.lower()
                if any(excluded in lower_title for excluded in ["web oracle", "stream", "chrome", "firefox", "edge", "browser", "visual studio", "cursor", "code", "powershell", "cmd.exe", "wt.exe"]):
                    return True
                if self.title_filter.lower() in lower_title:
                    found_hwnd = hwnd
                    return False
                return True

            try:
                if self.hdesk:
                    win32gui.EnumDesktopWindows(self.hdesk, enum_cb, None)
                else:
                    win32gui.EnumWindows(enum_cb, None)
            except Exception:
                pass

        self.hwnd = found_hwnd
        return found_hwnd

    def ensure_window(self) -> Optional[int]:
        if self.hwnd and user32.IsWindow(self.hwnd):
            return self.hwnd

        hwnd = self.find_window()
        if hwnd:
            return hwnd

        if not self.auto_launch:
            return None

        # Do not launch a duplicate if an existing process is already running
        if self.proc and self.proc.poll() is None:
            return None

        print("[KhepriOracle] Window not found. Looking for KhepriEngine executable to launch...")
        candidates = []
        if self.custom_exe:
            candidates.append(Path(self.custom_exe))

        candidates.extend([
            self.repo_root / "build" / "Debug" / "KhepriEngine.exe",
            self.repo_root / "build" / "Release" / "KhepriEngine.exe",
            self.repo_root / "build" / "RelWithDebInfo" / "KhepriEngine.exe",
            self.repo_root / "out" / "build" / "x64-Debug" / "KhepriEngine.exe",
            self.repo_root / "out" / "build" / "x64-Release" / "KhepriEngine.exe",
        ])

        exe_path = None
        for cand in candidates:
            if cand.exists() and cand.is_file():
                exe_path = cand
                break

        if not exe_path:
            print(f"[KhepriOracle] ERROR: Could not locate KhepriEngine.exe in candidate paths.")
            return None

        print(f"[KhepriOracle] Launching: {exe_path}")
        si = subprocess.STARTUPINFO()
        if self.virtual_desktop:
            si.lpDesktop = f"winsta0\\{self.virtual_desktop}"

        self.proc = subprocess.Popen([str(exe_path)], cwd=str(self.repo_root), startupinfo=si)

        # Wait up to 10 seconds for window creation
        for _ in range(20):
            time.sleep(0.5)
            hwnd = self.find_window()
            if hwnd:
                print(f"[KhepriOracle] Successfully connected to Khepri Engine (HWND: {hwnd})")
                return hwnd

        print("[KhepriOracle] Timed out waiting for Khepri window to appear.")
        return None

    def get_client_bounds(self) -> Optional[Tuple[int, int, int, int]]:
        """Returns (origin_x, origin_y, width, height) in screen pixel coordinates."""
        setup_thread_desktop(self.hdesk)
        if not self.hwnd or not user32.IsWindow(self.hwnd):
            self.hwnd = self.find_window()
            if not self.hwnd:
                return None

        rect = ctypes.wintypes.RECT()
        user32.GetClientRect(self.hwnd, ctypes.byref(rect))
        w = rect.right - rect.left
        h = rect.bottom - rect.top

        if w <= 0 or h <= 0:
            return None

        pt = ctypes.wintypes.POINT(0, 0)
        user32.ClientToScreen(self.hwnd, ctypes.byref(pt))

        return pt.x, pt.y, w, h

    def focus(self):
        if self.hwnd and user32.IsWindow(self.hwnd):
            user32.SetForegroundWindow(self.hwnd)

    def cleanup(self):
        """Terminates spawned child process and closes desktop handles on exit."""
        if self.proc:
            try:
                print("[KhepriOracle] Terminating spawned KhepriEngine process...")
                self.proc.terminate()
                self.proc.wait(timeout=2.0)
            except Exception:
                pass
        if self.hdesk:
            try:
                user32.CloseDesktop(self.hdesk)
            except Exception:
                pass


# ---------------------------------------------------------------------------
# Streaming & Input Server Engine
# ---------------------------------------------------------------------------
class WebOracleServer:
    def __init__(self, host: str, port: int, fps: int, quality: int, window_mgr: KhepriWindowManager):
        self.host = host
        self.port = port
        self.fps = fps
        self.quality = quality
        self.window_mgr = window_mgr
        self.pipe_client = OraclePipeClient()
        self.connected_clients: Set[ServerConnection] = set()
        self.current_frame_jpeg: Optional[bytes] = None
        self.last_bounds: Optional[Tuple[int, int, int, int]] = None
        self.static_dir = Path(__file__).resolve().parent / "static"
        self.running = True

    def inject_input(self, msg: dict):
        """Translates and injects web client mouse & keyboard events into Windows OS and Khepri Engine."""
        setup_thread_desktop(self.window_mgr.hdesk)
        bounds = self.window_mgr.get_client_bounds()
        if not bounds or not self.window_mgr.hwnd:
            return

        x0, y0, width, height = bounds
        hwnd = self.window_mgr.hwnd
        event_type = msg.get("type")
        is_virtual_desktop = self.window_mgr.virtual_desktop is not None

        if event_type == "mouse":
            action = msg.get("action")
            nx = float(msg.get("x", 0.0))
            ny = float(msg.get("y", 0.0))
            btn = int(msg.get("button", 0))
            client_x = int(nx * width)
            client_y = int(ny * height)
            screen_x = int(x0 + client_x)
            screen_y = int(y0 + client_y)
            lparam = ((client_y & 0xFFFF) << 16) | (client_x & 0xFFFF)

            # 1. Pipe Injection (Direct into ImGui - 100% reliable)
            if action == "move":
                self.pipe_client.send_event(1, 0, 0, nx, ny, 0.0)
            elif action == "down":
                self.pipe_client.send_event(2, btn, 0, nx, ny, 0.0)
            elif action == "up":
                self.pipe_client.send_event(3, btn, 0, nx, ny, 0.0)
            elif action == "wheel":
                delta_y = float(msg.get("deltaY", 0.0))
                wheel_val = 1.0 if delta_y < 0 else -1.0
                self.pipe_client.send_event(4, 0, 0, nx, ny, wheel_val)

            # 2. OS Fallback / Virtual Desktop Win32 Message
            if is_virtual_desktop:
                if action == "move":
                    user32.PostMessageW(hwnd, 0x0200, 0, lparam)
                elif action == "down":
                    if btn == 0: user32.PostMessageW(hwnd, 0x0201, 0x0001, lparam)
                    elif btn == 1: user32.PostMessageW(hwnd, 0x0207, 0x0010, lparam)
                    elif btn == 2: user32.PostMessageW(hwnd, 0x0204, 0x0002, lparam)
                elif action == "up":
                    if btn == 0: user32.PostMessageW(hwnd, 0x0202, 0, lparam)
                    elif btn == 1: user32.PostMessageW(hwnd, 0x0208, 0, lparam)
                    elif btn == 2: user32.PostMessageW(hwnd, 0x0205, 0, lparam)
                elif action == "wheel":
                    delta_y = msg.get("deltaY", 0)
                    wheel_delta = -120 if delta_y > 0 else 120
                    user32.PostMessageW(hwnd, 0x020A, (wheel_delta & 0xFFFF) << 16, lparam)
            else:
                user32.SetCursorPos(screen_x, screen_y)
                if action == "down":
                    self.window_mgr.focus()
                    if btn == 0: user32.mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0)
                    elif btn == 1: user32.mouse_event(MOUSEEVENTF_MIDDLEDOWN, 0, 0, 0, 0)
                    elif btn == 2: user32.mouse_event(MOUSEEVENTF_RIGHTDOWN, 0, 0, 0, 0)
                elif action == "up":
                    if btn == 0: user32.mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0)
                    elif btn == 1: user32.mouse_event(MOUSEEVENTF_MIDDLEUP, 0, 0, 0, 0)
                    elif btn == 2: user32.mouse_event(MOUSEEVENTF_RIGHTUP, 0, 0, 0, 0)
                elif action == "wheel":
                    delta_y = msg.get("deltaY", 0)
                    wheel_delta = -120 if delta_y > 0 else 120
                    user32.mouse_event(MOUSEEVENTF_WHEEL, 0, 0, wheel_delta, 0)

        elif event_type == "key":
            action = msg.get("action")
            code = msg.get("code", "")
            key_char = msg.get("key", "")
            ctrl = msg.get("ctrl", False)
            imgui_key = IMGUI_KEY_MAP.get(code, 0)

            # Direct Pipe key injection
            if imgui_key > 0:
                if action == "down":
                    self.pipe_client.send_event(5, 0, imgui_key, 0.0, 0.0, 0.0)
                    if len(key_char) == 1 and not ctrl and ord(key_char) >= 32:
                        self.pipe_client.send_event(7, 0, ord(key_char), 0.0, 0.0, 0.0)
                elif action == "up":
                    self.pipe_client.send_event(6, 0, imgui_key, 0.0, 0.0, 0.0)

            vk = VK_MAP.get(code)
            if vk:
                if is_virtual_desktop:
                    if action == "down":
                        user32.PostMessageW(hwnd, 0x0100, vk, 0)
                        if len(key_char) == 1 and not ctrl and ord(key_char) >= 32:
                            user32.PostMessageW(hwnd, 0x0102, ord(key_char), 0)
                    elif action == "up":
                        user32.PostMessageW(hwnd, 0x0101, vk, 0xC0000001)
                else:
                    flags = 0
                    if action == "up":
                        flags |= KEYEVENTF_KEYUP
                    if code in ["ArrowLeft", "ArrowUp", "ArrowRight", "ArrowDown", "Delete", "Insert", "Home", "End", "PageUp", "PageDown"]:
                        flags |= KEYEVENTF_EXTENDEDKEY
                    user32.keybd_event(vk, 0, flags, 0)

        elif event_type == "type_text":
            text = msg.get("text", "")
            for char in text:
                char_code = ord(char)
                self.pipe_client.send_event(7, 0, char_code, 0.0, 0.0, 0.0)
                if is_virtual_desktop:
                    user32.PostMessageW(hwnd, 0x0102, char_code, 0)
                else:
                    user32.keybd_event(0, char_code, 0x0004, 0)
                    user32.keybd_event(0, char_code, 0x0004 | KEYEVENTF_KEYUP, 0)

        elif event_type == "focus":
            self.window_mgr.focus()

    def capture_frame_gdi(self, hwnd: int, w: int, h: int) -> Optional[Image.Image]:
        """Captures window framebuffer via GDI PrintWindow and crops to client area (eliminating titlebar offset)."""
        win_rect = ctypes.wintypes.RECT()
        client_rect = ctypes.wintypes.RECT()
        user32.GetWindowRect(hwnd, ctypes.byref(win_rect))
        user32.GetClientRect(hwnd, ctypes.byref(client_rect))

        full_w = win_rect.right - win_rect.left
        full_h = win_rect.bottom - win_rect.top
        client_w = client_rect.right - client_rect.left
        client_h = client_rect.bottom - client_rect.top

        if full_w <= 0 or full_h <= 0 or client_w <= 0 or client_h <= 0:
            return None

        pt = ctypes.wintypes.POINT(0, 0)
        user32.ClientToScreen(hwnd, ctypes.byref(pt))

        offset_x = pt.x - win_rect.left
        offset_y = pt.y - win_rect.top

        hwnd_dc = win32gui.GetWindowDC(hwnd)
        if not hwnd_dc:
            return None

        mfc_dc = win32ui.CreateDCFromHandle(hwnd_dc)
        save_dc = mfc_dc.CreateCompatibleDC()
        save_bitmap = win32ui.CreateBitmap()
        save_bitmap.CreateCompatibleBitmap(mfc_dc, full_w, full_h)
        save_dc.SelectObject(save_bitmap)

        # PW_RENDERFULLCONTENT = 2
        user32.PrintWindow(hwnd, save_dc.GetSafeHdc(), 2)

        bmpinfo = save_bitmap.GetInfo()
        bmpstr = save_bitmap.GetBitmapBits(True)
        img = Image.frombuffer("RGB", (bmpinfo["bmWidth"], bmpinfo["bmHeight"]), bmpstr, "raw", "BGRX", 0, 1)

        win32gui.DeleteObject(save_bitmap.GetHandle())
        save_dc.DeleteDC()
        mfc_dc.DeleteDC()
        win32gui.ReleaseDC(hwnd, hwnd_dc)

        # Crop to the exact client viewport
        if 0 <= offset_x < full_w and 0 <= offset_y < full_h:
            img = img.crop((offset_x, offset_y, min(full_w, offset_x + client_w), min(full_h, offset_y + client_h)))

        return img

    async def capture_loop(self):
        """High framerate capture loop using MSS or GDI PrintWindow."""
        frame_interval = 1.0 / self.fps
        is_virtual = self.window_mgr.virtual_desktop is not None
        print(f"[KhepriOracle] Starting frame capture loop ({self.fps} FPS, quality {self.quality}, VirtualDesktop: {is_virtual})...")

        with mss.MSS() as sct:
            while self.running:
                start_time = time.perf_counter()

                if not self.connected_clients:
                    await asyncio.sleep(0.1)
                    continue

                bounds = self.window_mgr.get_client_bounds()
                if not bounds or not self.window_mgr.hwnd:
                    self.window_mgr.ensure_window()
                    await asyncio.sleep(0.5)
                    continue

                x0, y0, w, h = bounds

                # Broadcast metadata change if client size changed
                if self.last_bounds != (x0, y0, w, h):
                    self.last_bounds = (x0, y0, w, h)
                    meta_msg = json.dumps({"type": "metadata", "width": w, "height": h})
                    if self.connected_clients:
                        await asyncio.gather(*[client.send(meta_msg) for client in list(self.connected_clients)], return_exceptions=True)

                pil_img = None
                try:
                    if is_virtual:
                        pil_img = self.capture_frame_gdi(self.window_mgr.hwnd, w, h)
                    else:
                        try:
                            bbox = {"left": x0, "top": y0, "width": w, "height": h}
                            sct_img = sct.grab(bbox)
                            pil_img = Image.frombytes("RGB", sct_img.size, sct_img.bgra, "raw", "BGRX")
                        except Exception:
                            # Fallback to GDI capture
                            pil_img = self.capture_frame_gdi(self.window_mgr.hwnd, w, h)

                    if pil_img:
                        bio = io.BytesIO()
                        pil_img.save(bio, format="JPEG", quality=self.quality, optimize=False)
                        jpeg_bytes = bio.getvalue()
                        self.current_frame_jpeg = jpeg_bytes

                        if self.connected_clients:
                            await asyncio.gather(*[client.send(jpeg_bytes) for client in list(self.connected_clients)], return_exceptions=True)
                except Exception as e:
                    print(f"[KhepriOracle Capture Error] {e}", flush=True)

                elapsed = time.perf_counter() - start_time
                sleep_time = max(0.001, frame_interval - elapsed)
                await asyncio.sleep(sleep_time)

    async def handle_ws(self, websocket: ServerConnection):
        """Handles client connection and incoming interaction packets."""
        self.connected_clients.add(websocket)
        client_addr = websocket.remote_address
        print(f"[KhepriOracle] Browser client connected from {client_addr}. (Total active: {len(self.connected_clients)})")

        if self.last_bounds:
            _, _, w, h = self.last_bounds
            await websocket.send(json.dumps({"type": "metadata", "width": w, "height": h}))

        if self.current_frame_jpeg:
            await websocket.send(self.current_frame_jpeg)

        try:
            async for message in websocket:
                if isinstance(message, str):
                    try:
                        data = json.loads(message)
                        self.inject_input(data)
                    except Exception as err:
                        print(f"[KhepriOracle] Failed to process input event: {err}")
        except websockets.exceptions.ConnectionClosed:
            pass
        finally:
            self.connected_clients.remove(websocket)
            print(f"[KhepriOracle] Browser client disconnected: {client_addr}. (Remaining: {len(self.connected_clients)})")

    def handle_http_request(self, connection: ServerConnection, request: Request) -> Optional[Response]:
        """Serves static frontend files over HTTP on the same port."""
        path = request.path.split("?")[0]

        if path == "/ws" or request.headers.get("Upgrade", "").lower() == "websocket":
            return None

        if path == "/" or path == "/index.html":
            file_path = self.static_dir / "index.html"
        else:
            rel_path = path.lstrip("/")
            file_path = self.static_dir / rel_path

        if file_path.exists() and file_path.is_file():
            content_type, _ = mimetypes.guess_type(str(file_path))
            if not content_type:
                content_type = "application/octet-stream"

            try:
                body = file_path.read_bytes()
                headers = Headers()
                headers["Content-Type"] = content_type
                headers["Content-Length"] = str(len(body))
                headers["Cache-Control"] = "no-cache"
                return Response(200, "OK", headers, body)
            except Exception as e:
                err_body = f"Internal Server Error: {e}".encode("utf-8")
                headers = Headers()
                headers["Content-Type"] = "text/plain"
                headers["Content-Length"] = str(len(err_body))
                return Response(500, "Internal Server Error", headers, err_body)

        not_found = b"404 Not Found"
        headers = Headers()
        headers["Content-Type"] = "text/plain"
        headers["Content-Length"] = str(len(not_found))
        return Response(404, "Not Found", headers, not_found)

    async def run(self):
        # Initial check / auto-launch for Khepri Engine
        self.window_mgr.ensure_window()

        # Start WebSocket & HTTP combined server
        vdesktop_str = f" [Virtual Desktop: {self.window_mgr.virtual_desktop}]" if self.window_mgr.virtual_desktop else ""
        print(f"[KhepriOracle] ======================================================")
        print(f"[KhepriOracle]  Khepri Web Oracle Streaming Server Running!{vdesktop_str}")
        print(f"[KhepriOracle]  Open in Browser: http://localhost:{self.port}")
        print(f"[KhepriOracle] ======================================================")

        async with serve(self.handle_ws, self.host, self.port, process_request=self.handle_http_request):
            await self.capture_loop()


# ---------------------------------------------------------------------------
# Entry Point
# ---------------------------------------------------------------------------
def main():
    parser = argparse.ArgumentParser(description="Khepri Engine Web Oracle Streaming Server")
    parser.add_argument("--host", default="0.0.0.0", help="Host address to bind (default: 0.0.0.0)")
    parser.add_argument("--port", type=int, default=8080, help="Port to listen on (default: 8080)")
    parser.add_argument("--fps", type=int, default=30, help="Target framerate (default: 30)")
    parser.add_argument("--quality", type=int, default=75, help="JPEG compression quality 1-100 (default: 75)")
    parser.add_argument("--title", default="Khepri Engine", help="Window title substring to discover (default: Khepri Engine)")
    parser.add_argument("--no-launch", action="store_true", help="Disable automatic launching of KhepriEngine.exe if not running")
    parser.add_argument("--exe", default=None, help="Custom path to KhepriEngine.exe")
    parser.add_argument("--virtual-desktop", nargs="?", const="KhepriVirtualDesktop", default=None,
                        help="Run Khepri on a dedicated Win32 Virtual Desktop (default name: KhepriVirtualDesktop)")

    args = parser.parse_args()

    window_mgr = KhepriWindowManager(
        title_filter=args.title,
        auto_launch=not args.no_launch,
        custom_exe=args.exe,
        virtual_desktop=args.virtual_desktop
    )

    atexit.register(window_mgr.cleanup)

    server = WebOracleServer(
        host=args.host,
        port=args.port,
        fps=args.fps,
        quality=args.quality,
        window_mgr=window_mgr
    )

    try:
        asyncio.run(server.run())
    except KeyboardInterrupt:
        print("\n[KhepriOracle] Shutting down Web Oracle server.")
    finally:
        window_mgr.cleanup()


if __name__ == "__main__":
    main()
