import argparse
import asyncio
import json
import random
import datetime
import string
import struct
import threading
import queue
import tkinter as tk
from typing import Tuple, List

def make_msg_payload(user: str, text: str) -> bytes:
    body = json.dumps({
        "type": "message",
        "user_id": user,
        "payload": {"text": text},
        "timestamp": int(datetime.datetime.now().timestamp())
    }).encode("utf-8")
    return struct.pack("!I", len(body)) + body

def make_join_payload(channel_id: int) -> bytes:
    body = json.dumps({
        "type": "join",
        "channel_id": channel_id
    }).encode("utf-8")
    return struct.pack("!I", len(body)) + body

def random_text(min_len: int, max_len: int) -> str:
    length = random.randint(min_len, max_len)
    alphabet = string.ascii_letters + string.digits + " "
    return "".join(random.choice(alphabet) for _ in range(length)).strip()

class ChannelWindow:
    def __init__(self, root: tk.Tk, channel_id: int, q: queue.Queue):
        self.q = q
        self.win = tk.Toplevel(root)
        self.win.title(f"Channel {channel_id}")
        self.text = tk.Text(self.win, wrap="word", height=20, width=50)
        scroll = tk.Scrollbar(self.win, command=self.text.yview)
        self.text.configure(yscrollcommand=scroll.set)
        self.text.pack(side="left", fill="both", expand=True)
        scroll.pack(side="right", fill="y")

        self.text.tag_configure("msg", foreground="black")
        self.text.tag_configure("join", foreground="green")
        self.text.tag_configure("leave", foreground="red")
        self.text.tag_configure("status", foreground="gray")
        
        ts = datetime.datetime.now().strftime("%H:%M:%S")
        self.text.insert("end", f"[{ts}] [Channel {channel_id}] Active\n", ("status",))
        self.poll()

    def poll(self):
        try:
            while not self.q.empty():
                item = self.q.get()
                kind = item[0]
                timestamp = datetime.datetime.now().strftime("%H:%M:%S")
                if kind == "msg":
                    _, user, text = item
                    self.text.insert("end", f"[{timestamp}] [{user}] {text}\n", ("msg",))
                elif kind == "join":
                    _, user = item
                    self.text.insert("end", f"[{timestamp}] >> {user} joined\n", ("join",))
                elif kind == "leave":
                    _, user = item
                    self.text.insert("end", f"[{timestamp}] << {user} left\n", ("leave",))
                elif kind == "status":
                    _, msg = item
                    self.text.insert("end", f"[{timestamp}] [System] {msg}\n", ("status",))
                self.text.see("end")
        except Exception:
            pass
        finally:
            try:
                self.win.after(100, self.poll)
            except:
                pass

async def handle_client(idx: int, host: str, port: int, delay: Tuple[float, float], text_len: Tuple[int, int], channel_queues: List[queue.Queue]):
    user = f"user-{idx}"
    current_channel = -1 # 0-indexed

    try:
        reader, writer = await asyncio.open_connection(host, port)
    except Exception as e:
        print(f"[{user}] Connection failed: {e}")
        return

    # Initial join to a random channel (1-5)
    initial_ch = random.randint(0, 4)
    writer.write(make_join_payload(initial_ch + 1))
    await writer.drain()
    current_channel = initial_ch
    channel_queues[current_channel].put(("join", user))

    async def sender():
        nonlocal current_channel
        while True:
            await asyncio.sleep(random.uniform(*delay))
            
            # 1:9 ratio for Switch : Message
            action = random.random()
            if action < 0.1: # Switch
                new_ch = random.randint(0, 4)
                if new_ch != current_channel:
                    writer.write(make_join_payload(new_ch + 1))
                    await writer.drain()
                    
                    if current_channel != -1:
                        channel_queues[current_channel].put(("leave", user))
                    channel_queues[new_ch].put(("join", user))
                    
                    current_channel = new_ch
            else: # Message
                msg = random_text(*text_len)
                writer.write(make_msg_payload(user, msg))
                await writer.drain()

    async def receiver():
        while True:
            try:
                header = await reader.readexactly(4)
                (length,) = struct.unpack("!I", header)
                data = await reader.readexactly(length)
                
                try:
                    payload = json.loads(data.decode("utf-8"))
                    p_type = payload.get("type", "")
                    if p_type == "message":
                        from_user = payload.get("user_id", "?")
                        text = payload.get("payload", {}).get("text", "")
                        if current_channel != -1:
                            channel_queues[current_channel].put(("msg", from_user, text))
                except json.JSONDecodeError:
                    pass
            except (asyncio.IncompleteReadError, ConnectionResetError):
                break
            except Exception:
                break

    try:
        await asyncio.gather(sender(), receiver())
    except Exception as e:
        print(f"[{user}] Error: {e}")
    finally:
        if current_channel != -1:
            channel_queues[current_channel].put(("leave", user))
        try:
            writer.close()
            await writer.wait_closed()
        except:
            pass

def start_async_clients(args, channel_queues):
    async def runner():
        tasks = []
        for i in range(args.clients):
            tasks.append(asyncio.create_task(
                handle_client(i, args.host, args.port, (args.min_delay, args.max_delay), (args.min_len, args.max_len), channel_queues)
            ))
        await asyncio.gather(*tasks)
    asyncio.run(runner())

def main():
    parser = argparse.ArgumentParser(description="Tk GUI for multi channel simulation")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=4800)
    parser.add_argument("--clients", type=int, default=10, help="number of simulated clients")
    parser.add_argument("--min-delay", type=float, default=2.0)
    parser.add_argument("--max-delay", type=float, default=4.0)
    parser.add_argument("--min-len", type=int, default=5)
    parser.add_argument("--max-len", type=int, default=40)
    args = parser.parse_args()

    root = tk.Tk()
    root.withdraw()

    channel_queues = []
    windows = []
    for i in range(5): # 5 Channels
        q = queue.Queue()
        channel_queues.append(q)
        windows.append(ChannelWindow(root, i + 1, q))

    t = threading.Thread(target=start_async_clients, args=(args, channel_queues), daemon=True)
    t.start()

    def on_close():
        root.quit()

    root.protocol("WM_DELETE_WINDOW", on_close)
    root.mainloop()

if __name__ == "__main__":
    main()
