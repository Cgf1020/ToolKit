# 简单的 WebSocket 回显服务器
# 依赖：pip install websockets

import asyncio
import sys
import websockets

HOST = "127.0.0.1"
PORT = 8765

# 保存当前连接
active_clients = set()


async def handler(ws: websockets.WebSocketServerProtocol, path: str = "/"):
    active_clients.add(ws)
    print(f"[server] client connected: {ws.remote_address}")
    try:
        async for msg in ws:
            print(f"[server] recv: {msg}")
            await ws.send(f"[echo] {msg}")
    except websockets.ConnectionClosed as e:
        print(f"[server] connection closed: {e}")
    finally:
        active_clients.discard(ws)
        print(f"[server] client disconnected: {ws.remote_address}")


async def stdin_broadcast():
    loop = asyncio.get_running_loop()
    print("输入内容回车可广播；输入 exit 退出。")
    while True:
        line = await loop.run_in_executor(None, sys.stdin.readline)
        if not line:
            continue
        line = line.strip()
        if line == "exit":
            print("[server] exit requested, stopping...")
            for ws in list(active_clients):
                await ws.close(code=1000, reason="server exit")
            asyncio.get_event_loop().stop()
            return
        # 广播到所有连接
        if active_clients:
            await asyncio.gather(*(ws.send(f"[server broadcast] {line}") for ws in active_clients))
            print(f"[server] broadcast to {len(active_clients)} client(s)")
        else:
            print("[server] no active clients")


async def main():
    async with websockets.serve(handler, HOST, PORT):
        print(f"[server] listening on ws://{HOST}:{PORT}")
        # 并发运行 stdin 监听
        await stdin_broadcast()


if __name__ == "__main__":
    asyncio.run(main())
