import json
import asyncio
import socket
import random
import threading
import argparse

import websockets

parser = argparse.ArgumentParser()
parser.add_argument("--id", default="F0E1D2")
parser.add_argument("--port", type=int, default=81)
parser.add_argument("--freeze-after", type=float, default=None,
                    help="seconds after a client connects: stop sending data but keep the socket open")
parser.add_argument("--ignore-fan-commands", action="store_true",
                    help="accept fan_regulation_request but do not apply it (tests the timeout)")
args = parser.parse_args()

DISCOVERY_PORT = 45678
WS_PORT = args.port
DEVICE_ID = args.id

#Fan work mode: 1=auto, 2=turbo, 3=sleep, 4=manual

state = {"mode":1, "manual_speed": 70}

def status_frame(status):
    return {"type": "pairing_status",
            "payload": {"status": status, "device_id": DEVICE_ID}}

def current_speed(): 
        mode = state["mode"]
        if mode == 1:
             return random.randint(60,85)
        if mode == 2:
             return 100
        if mode == 3:
             return 45
        return state["manual_speed"]

def build_data_frame():
     return {
          "type": "data",
          "payload": {
               "sensor_reading": {
                    "pm25": random.randint(5,40),
                    "voc": round(random.uniform(0.1, 0.6), 1),
                    "co2": random.randint(500, 900),
                    "temperature": round(random.uniform(21.0, 24.0), 1),
                    "humidity": random.randint(40, 55),
               },
               "fan_state": {
                    "current_mode": state["mode"],
                    "current_speed": current_speed(),
               },
               "filter_state": {"filter_life_percentage": 65},
          } 
     }

def status_frame(status): 
      return {
        "type": "connection_status",
        "payload": {"status": status, "device_id": DEVICE_ID},
    }

def discovery_loop():
     sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
     sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
     sock.bind(("", DISCOVERY_PORT))
     print(f"[UDP] Listening on: {DISCOVERY_PORT}")
     while True:
            data, sender = sock.recvfrom(1024)
            try:
                 message = json.loads(data)
            except json.JSONDecodeError:
                 continue
            if not isinstance(message, dict) or message.get("type") != "discover":
                 continue
            reply = {
                "type": "discover_reply",
                "payload": {"device_id": DEVICE_ID, "ws_port": WS_PORT},
            }
            sock.sendto(json.dumps(reply).encode(), sender)
            print(f"[UDP] Responed to discover from: {sender}")

async def send_data_loop(websocket):
    loop = asyncio.get_running_loop()
    started = loop.time()
    try:
        while True:
            frozen = args.freeze_after is not None and loop.time() - started >= args.freeze_after
            if not frozen:
                await websocket.send(json.dumps(build_data_frame()))
            await asyncio.sleep(2)
    except websockets.ConnectionClosed:
        pass
    except Exception as error:
        print(f"[WS] error in send loop: {error!r}")

async def handle_message(websocket, raw):
    try:
        message = json.loads(raw)
    except json.JSONDecodeError:
        return
    if not isinstance(message, dict):
        return
   

    kind = message.get("type")
    payload = message.get("payload") or {}

    if kind == "fan_regulation_request":
            if args.ignore_fan_commands:
                print("[WS] fan command ignored (test)")
                return
            state["mode"] = payload.get("new_mode", state["mode"])

    if kind == "fan_regulation_request":
        state["mode"] = payload.get("new_mode", state["mode"])
        speed = payload.get("requested_speed")
        if state["mode"] == 4 and speed is not None:
            state["manual_speed"] = max(60, min(85, int(speed)))
        print(f"[WS] mode changed to: {state}")
    
    elif kind == "credentials_submit":
        ssid = payload.get("ssid", "")
        password = payload.get("password", "")
        print(f"[WS] WiFi credentials: ssid={ssid!r}")
        await websocket.send(json.dumps(status_frame(0)))  # łączenie w toku
        await asyncio.sleep(1.5)
        if ssid == "timeout":
            return  
        if password == "wrong": # złe dane
            status = 2
        elif ssid == "nonet": # błąd sieci
            status = 3
        else:
            status = 1 #gitara gra
        await websocket.send(json.dumps(status_frame(status)))


async def handler(websocket):
    print("[WS] client connected successfully")
    sender = asyncio.create_task(send_data_loop(websocket))
    try:
        async for raw in websocket:
            await handle_message(websocket, raw)
    except websockets.ConnectionClosed:
        pass
    finally:
        sender.cancel()
        print("[WS] client disconnected")


async def main():
    threading.Thread(target=discovery_loop, daemon=True).start()
    async with websockets.serve(handler, "0.0.0.0", WS_PORT):
        print(f"[WS] server on :{WS_PORT}, device {DEVICE_ID}")
        await asyncio.Future()  # dziala w nieskonczonosc


try:
    asyncio.run(main())
except KeyboardInterrupt:
    print("Fin")
