# cpp-chat-channel

서버에서 유저 접속 대기열 push > 유저가 send로 채널 입력 > 서버가 각 채널 존재 여부 파악해서 channel 스레드 생성(생성자에서 thread worker로 작동)

메인 스레드 - ChannelServer
서브 스레드 - Channel

## Request/Response 명세 v1

**매 요청/응답마다 raw string header로 4자리 16진수의 길이가 들어옴.**

- 신규 접속 및 채널 변경

```
//REQ:
{
	type: "join", // Join, JOIN
	user_name?: string,
	channel_id: int,
	timestamp: int
}

//RES:
{
	type: "system",
	event: "join" | "rejoin",
	user_name: string,
	timestamp: int,
	channel_id: int
}
```

- 채널 퇴장

```
//RES:
{
	type: "system",
	event: "leave",
	user_name: string,
	timestamp: int
}
```

- 메시지 전송

```
//REQ:
{
	type: "message", // Message, MESSAGE
	text: string,
	timestamp: int
}

//RES:
{
	type: "user",
	event: string, // message filtered and processed by server from user
	user_name: string,
	timestamp: int
}
```

- 에러

```
{
	type: "error",
	message: string // server/channel full, etc.
}
```

## Request/Response 명세 v2

**Raw RCP의 경우 매 요청/응답마다 4자리 16진수의 패킷 총 길이가 헤더로 추가되어있어야 함.**

- 신규 접속 및 채널 변경

```
//REQ:
{
	type: "join", // Join, JOIN
	user_name?: string,
	channel_id: int,
}

//RES:
{
	type: "system",
	event: "join" | "rejoin",
	user_name: string,
	channel_id: int
}
```

- 채널 퇴장

```
//RES:
{
	type: "system",
	event: "leave",
	user_name: string,
}
```

- 메시지 전송

```
//REQ:
{
	type: "message", // Message, MESSAGE
	text: string,
}

//RES:
{
	type: "user",
	event: string, // message filtered and processed by server from user
	user_name: string,
}
```

- 에러

```
{
	type: "error",
	message: string // server/channel full, etc.
}
```

- Raw TCP Ping-Pong

```
//Ping from Server:
0001-

//Pong from Client:
0002{}
```

## TODO:

1. JWT 검증
2. XSS 방지
3. rate limit
4. .h, .cpp 의존성 분리
5. .h에서 선언만 이용
6. logger
7. black list
8. Server class 구조 변경: json 기반 통신을 컴포넌트로 분리 (bytes, yaml, xml로 확장 가능하게)
