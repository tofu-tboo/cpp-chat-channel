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

def make_header(body: bytes) -> bytes:
    return f"{len(body):04x}".encode("utf-8")

def make_msg_payload(text: str) -> bytes:
    body = json.dumps({
        "type": "message",
        "text": text,
        "timestamp": int(datetime.datetime.now().timestamp() * 1000)
    }).encode("utf-8")
    return make_header(body) + body

def make_join_payload(channel_id: int, user_name: str) -> bytes:
    body = json.dumps({
        "type": "join",
        "user_name": user_name,
        "channel_id": channel_id,
        "timestamp": int(datetime.datetime.now().timestamp() * 1000)
    }).encode("utf-8")
    return make_header(body) + body

def random_text(min_len: int, max_len: int) -> str:
    length = random.randint(min_len, max_len)
    alphabet = string.ascii_letters + string.digits + " "
    return "".join(random.choice(alphabet) for _ in range(length)).strip()

NAMES = [
    "Alice", "Bob", "Charlie", "David", "Eve", "Frank", "Grace", "Heidi",
    "Ivan", "Judy", "Mallory", "Oscar", "Peggy", "Sybil", "Trent", "Walter"
]

CHANNEL_COLORS = {
    1: "#ffcccc", # Red
    2: "#ffebcc", # Orange
    3: "#ffffcc", # Yellow
    4: "#ccffcc", # Green
    5: "#cce5ff", # Blue
}

class UserWindow:
    def __init__(self, root: tk.Tk, user_name: str, q: queue.Queue, stop_event: threading.Event):
        self.q = q
        self.user_name = user_name
        self.stop_event = stop_event
        self.win = tk.Toplevel(root)
        self.win.title(f"Client: {user_name}")
        self.win.geometry("400x500")
        self.win.protocol("WM_DELETE_WINDOW", self.on_close)
        
        self.text = tk.Text(self.win, wrap="word", state="disabled", bg="white")
        scroll = tk.Scrollbar(self.win, command=self.text.yview)
        self.text.configure(yscrollcommand=scroll.set)
        
        self.text.pack(side="left", fill="both", expand=True)
        scroll.pack(side="right", fill="y")

        # Tags for alignment and coloring
        self.text.tag_configure("self", justify="right", foreground="blue", lmargin1=50)
        self.text.tag_configure("other", justify="left", foreground="black", rmargin=50)
        self.text.tag_configure("system", justify="center", foreground="gray", font=("Helvetica", 8, "italic"))
        
        self.poll()

    def on_close(self):
        self.stop_event.set()
        self.win.destroy()

    def add_line(self, text: str, tags: Tuple[str]):
        self.text.configure(state="normal")
        self.text.insert("end", text + "\n", tags)
        self.text.see("end")
        self.text.configure(state="disabled")

    def poll(self):
        try:
            while not self.q.empty():
                item = self.q.get()
                kind = item[0]
                ts = datetime.datetime.now().strftime("%H:%M:%S")
                
                if kind == "user":
                    _, user, text = item
                    if user == self.user_name:
                        self.add_line(f"[{ts}] Me: {text}", ("self",))
                    else:
                        self.add_line(f"[{ts}] {user}: {text}", ("other",))
                elif kind == "system":
                    _, event, user, ch_id = item
                    self.add_line(f"[{ts}] [System] {user} {event}", ("system",))
                    
                    if user == self.user_name and event in ["join", "rejoin"]:
                        color = CHANNEL_COLORS.get(ch_id, "white")
                        self.text.configure(bg=color)
                elif kind == "error":
                    _, event = item
                    self.add_line(f"[{ts}] [System] {event}", ("system",))
        except Exception:
            pass
        finally:
            try:
                if not self.stop_event.is_set():
                    self.win.after(100, self.poll)
            except:
                pass

async def handle_client(idx: int, host: str, port: int, delay: Tuple[float, float], text_len: Tuple[int, int], q: queue.Queue, stop_event: threading.Event):
    user_name = f"{NAMES[idx % len(NAMES)]}-{idx}"
    current_channel = -1 # 0-indexed

    try:
        reader, writer = await asyncio.open_connection(host, port)
    except Exception as e:
        q.put(("system", f"Connection failed: {e}"))
        return

    # Initial join to a random channel (1-5)
    initial_ch = random.randint(1, 5)
    writer.write(make_join_payload(initial_ch, user_name))
    await writer.drain()
    current_channel = initial_ch

    async def sender():
        nonlocal current_channel
        while not stop_event.is_set():
            # Responsive sleep: check stop_event frequently
            sleep_duration = random.uniform(*delay)
            end_time = asyncio.get_running_loop().time() + sleep_duration
            while True:
                now = asyncio.get_running_loop().time()
                if now >= end_time or stop_event.is_set():
                    break
                await asyncio.sleep(min(0.1, end_time - now))
            
            if stop_event.is_set(): return
            
            # 1:9 ratio for Switch : Message
            action = random.random()
            if action < 0.1: # Switch
                new_ch = random.randint(1, 5)
                if new_ch != current_channel:
                    writer.write(make_join_payload(new_ch, user_name))
                    await writer.drain()
                    current_channel = new_ch
            else: # Message
                msg = random_text(*text_len)
                writer.write(make_msg_payload(msg))
                await writer.drain()

    async def receiver():
        while True:
            try:
                header = await reader.readexactly(4)
                try:
                    length = int(header.decode(), 16)
                except ValueError:
                    continue

                data = await reader.readexactly(length)
                
                if length == 1 and data == b'-':
                    writer.write(make_header(b'{}') + b'{}')
                    await writer.drain()
                    continue
                
                try:
                    root = json.loads(data.decode("utf-8"))
                    if isinstance(root, list):
                        payloads = root
                    else:
                        payloads = [root]
                    
                    for payload in payloads:
                        p_type = payload.get("type", "")
                        
                        if p_type == "user":
                            from_user = payload.get("user_name", "?")
                            text = payload.get("event", "")
                            q.put(("user", from_user, text))
                                
                        elif p_type == "system":
                            event = payload.get("event", "")
                            target_user = payload.get("user_name", "")
                            q.put(("system", event, target_user, current_channel))

                        elif p_type == "error":
                            message = payload.get("message", "Unknown error")
                            q.put(("error", f"Error from server: {message}"))
                        
                except json.JSONDecodeError:
                    pass
            except (asyncio.IncompleteReadError, ConnectionResetError):
                q.put(("system", "Disconnected"))
                break
            except Exception as e:
                q.put(("system", f"Error: {e}"))
                break

    sender_task = asyncio.create_task(sender())
    receiver_task = asyncio.create_task(receiver())

    # Wait until either sender finishes (window closed) or receiver finishes (connection lost)
    done, pending = await asyncio.wait(
        [sender_task, receiver_task],
        return_when=asyncio.FIRST_COMPLETED
    )

    for task in pending:
        task.cancel()
        try:
            await task
        except asyncio.CancelledError:
            pass

    try:
        writer.close()
        await writer.wait_closed()
    except:
        pass

def start_async_clients(args, user_queues, stop_events):
    async def runner():
        tasks = []
        for i in range(args.clients):
            tasks.append(asyncio.create_task(
                handle_client(i, args.host, args.port, (args.min_delay, args.max_delay), (args.min_len, args.max_len), user_queues[i], stop_events[i])
            ))
        await asyncio.gather(*tasks)
    asyncio.run(runner())

def main():
    parser = argparse.ArgumentParser(description="Tk GUI for multi channel simulation")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=4800)
    parser.add_argument("--clients", type=int, default=5, help="number of simulated clients")
    parser.add_argument("--min-delay", type=float, default=2.0)
    parser.add_argument("--max-delay", type=float, default=4.0)
    parser.add_argument("--min-len", type=int, default=5)
    parser.add_argument("--max-len", type=int, default=40)
    args = parser.parse_args()

    root = tk.Tk()
    root.withdraw()

    user_queues = []
    stop_events = []
    windows = []
    for i in range(args.clients):
        q = queue.Queue()
        stop = threading.Event()
        user_queues.append(q)
        stop_events.append(stop)
        windows.append(UserWindow(root, f"{NAMES[i % len(NAMES)]}-{i}", q, stop))

    t = threading.Thread(target=start_async_clients, args=(args, user_queues, stop_events), daemon=True)
    t.start()

    def on_close():
        root.quit()

    root.protocol("WM_DELETE_WINDOW", on_close)
    root.mainloop()

if __name__ == "__main__":
    main()
