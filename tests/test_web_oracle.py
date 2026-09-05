import asyncio
import json
import urllib.request
import websockets
import sys
from pathlib import Path

# Add project root to sys.path
repo_root = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(repo_root))

from tools.web_oracle.server import WebOracleServer, KhepriWindowManager

async def main():
    print("[Test] Starting WebOracle test server with Virtual Desktop support...", flush=True)
    window_mgr = KhepriWindowManager(title_filter='Khepri Engine', auto_launch=False, virtual_desktop='KhepriTestDesktop')
    server = WebOracleServer(host='127.0.0.1', port=8097, fps=15, quality=60, window_mgr=window_mgr)

    # Start server in background task
    server_task = asyncio.create_task(server.run())
    await asyncio.sleep(0.5)

    try:
        # 1. Test HTTP static file serving
        print("[Test] Testing HTTP GET /...", flush=True)
        def http_get(path="/"):
            with urllib.request.urlopen(f"http://127.0.0.1:8097{path}", timeout=3.0) as resp:
                return resp.status, resp.read().decode("utf-8")

        status, html = await asyncio.to_thread(http_get, "/")
        assert status == 200, f"Expected status 200, got {status}"
        assert 'Khepri Engine - Web Oracle Stream' in html
        assert 'stream-canvas' in html
        print('[PASS] HTTP GET / returned 200 OK and valid canvas markup', flush=True)

        # 2. Test WebSocket connection & bidirectional events
        print("[Test] Connecting WebSocket...", flush=True)
        async with websockets.connect('ws://127.0.0.1:8097/ws') as ws:
            print('[PASS] WebSocket connected successfully', flush=True)
            
            # Test sending mouse move
            print("[Test] Sending mouse move...", flush=True)
            await ws.send(json.dumps({'type': 'mouse', 'action': 'move', 'x': 0.5, 'y': 0.5}))
            print('[PASS] Sent mouse move', flush=True)

            # Test sending mouse click
            print("[Test] Sending mouse click...", flush=True)
            await ws.send(json.dumps({'type': 'mouse', 'action': 'down', 'button': 0, 'x': 0.5, 'y': 0.5}))
            await ws.send(json.dumps({'type': 'mouse', 'action': 'up', 'button': 0, 'x': 0.5, 'y': 0.5}))
            print('[PASS] Sent mouse click (down/up)', flush=True)

            # Test sending mouse wheel
            print("[Test] Sending mouse wheel...", flush=True)
            await ws.send(json.dumps({'type': 'mouse', 'action': 'wheel', 'deltaY': 100, 'x': 0.5, 'y': 0.5}))
            print('[PASS] Sent mouse wheel scroll', flush=True)

            # Test sending keypress
            print("[Test] Sending keypress...", flush=True)
            await ws.send(json.dumps({'type': 'key', 'action': 'down', 'code': 'Space'}))
            await ws.send(json.dumps({'type': 'key', 'action': 'up', 'code': 'Space'}))
            print('[PASS] Sent keyboard key (Space)', flush=True)

        print('[ALL ORACLE TESTS WITH VIRTUAL DESKTOP PASSED SUCCESSFULLY]', flush=True)

    finally:
        server.running = False
        server_task.cancel()
        try:
            await server_task
        except asyncio.CancelledError:
            pass
        window_mgr.cleanup()

if __name__ == '__main__':
    asyncio.run(main())
