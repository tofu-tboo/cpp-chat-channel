# cpp-chat-channel

서버에서 유저 접속 대기열 push > 유저가 send로 채널 입력 > 서버가 각 채널 존재 여부 파악해서 스레드 생성

## Request/Response 명세

**매 요청/응답마다 raw string header로 4자리 16진수의 길이가 들어옴.**

- 신규 접속 및 채널 변경

```
//REQ:
{
	type: "join", // Join, JOIN
	user_id: string,
	channel_id: int
}

//RES:
{
	type: "system",
	event: "join" | "rejoin",
	user_id: string
}
```

- 메시지 전송

```
//REQ:
{
	type: "message", // Message, MESSAGE
	text: string
}

//RES:
{
	type: "user",
	user_id: string,
	event: string, // message filtered and processed by server from user
	timestamp: int
}
```
