# cpp-chat-channel

서버에서 유저 접속 대기열 push > 유저가 send로 채널 입력 > 서버가 각 채널 존재 여부 파악해서 channel 스레드 생성(생성자에서 thread worker로 작동)

메인 스레드 - ChannelServer
서브 스레드 - Channel

## Request/Response 명세

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
	timestamp: int
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
