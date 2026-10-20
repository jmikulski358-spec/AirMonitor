import asyncio
import websockets

async def main():
    async with websockets.connect("ws://localhost:81") as ws:
        for _ in range(3):
            try:
                msg = await asyncio.wait_for(ws.recv(), timeout=5)
            except asyncio.TimeoutError:
                print("No frame for 5 s")
                return
            print(msg)

asyncio.run(main())