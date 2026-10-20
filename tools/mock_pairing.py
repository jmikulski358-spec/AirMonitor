import argparse
import asyncio
import json

import websockets


def make_status(status, device_id):
    return json.dumps({
        "type": "pairing_status",
        "payload": {"status": status, "device_id": device_id},
    })


async def handle_credentials(websocket, payload, device_id, delay):
    ssid = payload.get("ssid")
    password = payload.get("password")
    print(f"credentials received: ssid={ssid!r} password={password!r}")

    if ssid == "silent":
        print("scenario: silent -> no reply")
        return

    if password == "wrong":
        print("scenario: wrong password -> status 2")
        await websocket.send(make_status(2, device_id))
        return

    if ssid == "unreachable":
        print("scenario: network error -> status 3")
        await websocket.send(make_status(3, device_id))
        return

    print("scenario: success -> status 0, then 1, then close")
    await websocket.send(make_status(0, device_id))
    await asyncio.sleep(delay)
    await websocket.send(make_status(1, device_id))
    await asyncio.sleep(0.5)
    await websocket.close()


def make_handler(device_id, delay):
    # The closure lets the handler see device_id and delay, because
    # websockets calls it with the connection object only.
    async def handler(websocket, *_):
        print("client connected")
        try:
            async for raw in websocket:
                try:
                    frame = json.loads(raw)
                except json.JSONDecodeError:
                    print("not valid JSON:", raw)
                    continue

                if frame.get("type") != "credentials_submit":
                    print("unknown frame type:", frame.get("type"))
                    continue

                await handle_credentials(websocket, frame.get("payload", {}),
                                         device_id, delay)
        except websockets.exceptions.ConnectionClosed:
            pass
        print("client disconnected")

    return handler


async def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", type=int, default=8765)
    parser.add_argument("--id", default="F0E1D2", help="device_id reported on success")
    parser.add_argument("--delay", type=float, default=2.0,
                        help="seconds between status 0 and status 1")
    args = parser.parse_args()

    async with websockets.serve(make_handler(args.id, args.delay), "0.0.0.0", args.port):
        print(f"pairing mock listening on port {args.port}, device_id={args.id}")
        await asyncio.Future()  # run forever


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        pass