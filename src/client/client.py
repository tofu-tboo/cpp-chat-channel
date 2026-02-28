import asyncio
import websockets
import json
import sys
import argparse
from concurrent.futures import ThreadPoolExecutor

# ANSI colors for terminal output
class Colors:
    HEADER = '\033[95m'
    BLUE = '\033[94m'
    CYAN = '\033[96m'
    GREEN = '\033[92m'
    WARNING = '\033[93m'
    FAIL = '\033[91m'
    ENDC = '\033[0m'
    BOLD = '\033[1m'
    UNDERLINE = '\033[4m'

async def receive_messages(websocket):
    try:
        async for message in websocket:
            try:
                data = json.loads(message)
                msg_type = data.get("type", "unknown")
                
                if msg_type == "user":
                    user = data.get("user_name", "unknown")
                    text = data.get("event", "")
                    print(f"{Colors.CYAN}[{user}]{Colors.ENDC}: {text}")
                elif msg_type == "system":
                    text = data.get("event", "")
                    print(f"{Colors.YELLOW}[System]{Colors.ENDC}: {text}")
                elif msg_type == "error":
                    text = data.get("message", "")
                    print(f"{Colors.FAIL}[Error]{Colors.ENDC}: {text}")
                else:
                    print(f"{Colors.HEADER}[Raw]{Colors.ENDC}: {message}")
            except json.JSONDecodeError:
                print(f"{Colors.HEADER}[Raw]{Colors.ENDC}: {message}")
    except websockets.exceptions.ConnectionClosed:
        print(f"{Colors.FAIL}Connection closed by server.{Colors.ENDC}")
    except Exception as e:
        print(f"{Colors.FAIL}Error receiving message: {e}{Colors.ENDC}")

async def send_messages(websocket):
    loop = asyncio.get_event_loop()
    # Use ThreadPoolExecutor to run input() in a separate thread
    # so it doesn't block the asyncio loop
    with ThreadPoolExecutor(1, "AsyncInput") as executor:
        while True:
            try:
                # Run input() in a separate thread
                msg = await loop.run_in_executor(executor, input)
                
                if not msg:
                    continue
                
                if msg.lower() == "/quit":
                    print("Quitting...")
                    await websocket.close()
                    break
                
                # Simple command parsing
                payload = {}
                if msg.startswith("/join "):
                    parts = msg.split(" ")
                    if len(parts) > 1:
                        try:
                            channel_id = int(parts[1])
                            payload = {
                                "type": "join",
                                "channel_id": channel_id
                            }
                        except ValueError:
                            print(f"{Colors.FAIL}Invalid channel ID.{Colors.ENDC}")
                            continue
                else:
                    # Default to sending a message
                    payload = {
                        "type": "message",
                        "text": msg,
                        "timestamp": 0 # Server will handle timestamp
                    }
                
                if payload:
                    await websocket.send(json.dumps(payload))
                    
            except EOFError:
                break
            except Exception as e:
                print(f"{Colors.FAIL}Error sending message: {e}{Colors.ENDC}")
                break

async def main():
    parser = argparse.ArgumentParser(description="WebSocket Chat Client")
    parser.add_argument("--host", default="127.0.0.1", help="Server host (default: 127.0.0.1)")
    parser.add_argument("--port", type=int, default=4800, help="Server port (default: 4800)")
    parser.add_argument("--protocol", default="ws", choices=["ws", "wss"], help="Protocol (ws/wss)")
    
    args = parser.parse_args()
    
    uri = f"{args.protocol}://{args.host}:{args.port}"
    print(f"{Colors.GREEN}Connecting to {uri}...{Colors.ENDC}")
    
    try:
        async with websockets.connect(uri) as websocket:
            print(f"{Colors.GREEN}Connected! Type '/join <channel_id>' to switch channels or just type to chat.{Colors.ENDC}")
            await asyncio.gather(receive_messages(websocket), send_messages(websocket))
    except Exception as e:
        print(f"{Colors.FAIL}Failed to connect: {e}{Colors.ENDC}")

if __name__ == "__main__":
    asyncio.run(main())
