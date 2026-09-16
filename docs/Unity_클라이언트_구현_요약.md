# Unity 클라이언트 구현 요약

기준: 2026-09-16, 현재 zpd-server 소스. **방 입장·참가자 표시·양방향 채팅** 구현을 위한 인계 문서입니다. Unity 프로젝트 코드를 구현하거나 실행 검증한 문서는 아닙니다.

## 1. 연결과 구현 범위

- 전송: **TCP + 커스텀 8바이트 헤더 + Protobuf 본문**. HTTP, WebSocket, gRPC 프로토콜이 아닙니다.
- 기본 주소: 서버와 같은 PC에서는 `127.0.0.1:20000`. 다른 기기에서는 서버 PC의 접근 가능한 IP를 설정합니다.
- 연결 성공만으로 입장이 완료되지 않습니다. 반드시 `EnterRequest`의 성공 응답을 받은 뒤 방 기능을 사용합니다.
- 개발용 임시 플레이어 ID만 발급합니다. 계정 인증, 자동 재접속 복구, 채팅 기록, 방 목록 조회, 게임 이동·전투는 없습니다.
- `playerId`와 `roomId`는 C# `ulong`, `requestId`와 `capacity`는 `uint`입니다. ID를 `int`로 축소하지 않습니다.
- UI의 “방 정원”, “방 ID”, “플레이어 ID”를 구분합니다. 정원 2로 만든 첫 방은 **방 ID 1**일 수 있습니다.

## 2. 패킷 헤더

| 오프셋 | 필드 | 타입 | 규칙 |
|---|---|---|---|
| 0~1 | packetSize | uint16 | 헤더를 포함한 전체 길이, big-endian |
| 2 | messageCode | byte | 아래 메시지 코드 |
| 3 | errorCode | byte | 요청·알림은 0, 응답 오류는 아래 표 |
| 4~7 | requestId | uint32 | big-endian |
| 8 이후 | payload | byte[] | 해당 메시지의 Protobuf 직렬화 결과 |

- 전체 길이: **8~4096바이트**, 본문 최대 **4088바이트**.
- big-endian 규칙은 커스텀 헤더에 적용합니다. Protobuf 본문은 Google.Protobuf가 직렬화한 바이트를 그대로 사용합니다.
- 요청 ID는 연결마다 1부터 발급하며 0과 미완료 요청의 ID를 건너뜁니다. 응답은 요청 ID를 그대로 반환하고 알림은 항상 0입니다.
- 오류 응답은 본문이 비어 있습니다. 정상 Protobuf 본문으로 파싱하기 전에 오류부터 확인합니다.
- `EnterRequest`, `PingRequest`, `LeaveRoomRequest`, 성공 `ChatResponse` 등은 빈 메시지여서 본문 길이 0이 정상입니다.
- `BitConverter`의 기본 바이트 순서나 C# 구조체 메모리 배치를 그대로 전송하지 않습니다. Protobuf의 별도 길이 접두어도 덧붙이지 않습니다.

Ping 요청, requestId=1의 검증용 바이트:

```text
요청: 00 08 02 00 00 00 00 01
응답: 00 08 82 00 00 00 00 01
```

## 3. Protobuf 준비와 메시지 목록

서버의 다음 파일을 **그대로** 사용해 C# 코드를 생성합니다. 메시지 필드를 수동으로 다시 정의하지 않습니다.

- `common/proto/echo.proto`: Echo, Ping
- `common/proto/session.proto`: 개발용 입장
- `common/proto/room.proto`: 방 생성·입장·퇴장, 참가자 알림
- `common/proto/chat.proto`: 채팅

생성 예시(서버 저장소 루트에서, `Generated` 폴더를 먼저 생성):

```powershell
protoc --proto_path=proto --csharp_out=Generated echo.proto session.proto room.proto chat.proto
```

생성된 `.cs`와 호환되는 `Google.Protobuf` 런타임 및 해당 버전의 의존성을 Unity 프로젝트에 포함합니다. Unity 버전·API Compatibility Level·빌드 대상에 맞는 패키지 구성을 선택하고 실제 Player 빌드에서도 확인합니다. 현재 `package protocol`의 기본 C# 네임스페이스는 `Protocol`이며 필드 접근자는 `PlayerId`, `RoomId`, `PlayerIds`, `Capacity`, `Text`입니다. 생성 코드는 직접 수정하지 않습니다. [공식 C# 생성 코드 안내](https://protobuf.dev/reference/csharp/csharp-generated/)

| 기능 | 요청 → 응답 코드 | 요청 본문 → 성공 응답 본문 |
|---|---|---|
| Echo | 0x01 → 0x81 | EchoRequest: bytes data → EchoResponse: bytes data |
| Ping | 0x02 → 0x82 | PingRequest: 빈 메시지 → PingResponse: 빈 메시지 |
| 개발용 입장 | 0x03 → 0x83 | EnterRequest: 빈 메시지 → EnterResponse: uint64 player_id |
| 방 생성 | 0x04 → 0x84 | CreateRoomRequest: uint32 capacity → CreateRoomResponse: room_id, capacity, player_ids |
| 방 입장 | 0x05 → 0x85 | JoinRoomRequest: uint64 room_id → JoinRoomResponse: room_id, capacity, player_ids |
| 방 퇴장 | 0x06 → 0x86 | LeaveRoomRequest: 빈 메시지 → LeaveRoomResponse: uint64 room_id |
| 채팅 | 0x07 → 0x87 | ChatRequest: string text → ChatResponse: 빈 메시지 |

방 생성/입장 응답의 타입은 `uint64 room_id`, `uint32 capacity`, `repeated uint64 player_ids`입니다. 참가자 목록에는 **자신도 포함**됩니다.

| 알림 코드 | 본문 타입 | 필드와 처리 |
|---|---|---|
| 0xC1 | PlayerJoined | room_id, player_id → 해당 참가자 추가 |
| 0xC2 | PlayerLeft | room_id, player_id → 해당 참가자 제거 |
| 0xC3 | ChatMessage | room_id, player_id, text → 발신자와 메시지 표시 |
| 0xFF | ErrorResponse | 알 수 없는 요청·잘못된 방향에 대한 오류 응답. 본문 없음, 원래 요청 ID |

`ErrorResponse`는 알림이 아닙니다. 응답·알림 코드를 클라이언트 요청으로 보내면 서버가 거절합니다.

## 4. 상태와 UI 동작

클라이언트에서 관리할 상태 예시:

```text
Disconnected → AwaitingEntry → Lobby → InRoom
                                  ↑       │
                                  └───────┘  Leave 성공
어떤 상태에서든 연결 종료 → Disconnected
```

| 사용자 동작/서버 이벤트 | 클라이언트 처리 |
|---|---|
| TCP 연결 성공 | AwaitingEntry. EnterRequest 전송 |
| EnterResponse 성공 | PlayerId 저장, Lobby 전환 |
| 방 생성 버튼 | capacity=1~16으로 요청. 생성자가 자동 참가하므로 Join을 추가로 보내지 않음 |
| 방 입장 버튼 | 서버가 반환하거나 사용자가 입력한 roomId로 요청 |
| Create/Join 성공 | RoomId·Capacity 저장, 참가자 목록을 응답 전체 목록으로 교체, InRoom 전환 |
| PlayerJoined/PlayerLeft | 현재 방의 참가자 집합 갱신. 타 방·잘못된 ID 알림은 규격 오류로 처리 |
| 채팅 전송 버튼 | InRoom에서 ChatRequest.Text 전송 |
| ChatResponse 성공 | 전송 요청 완료 처리. 채팅 UI에 메시지를 추가하는 용도로 사용하지 않음 |
| ChatMessage | 발신자 ID와 텍스트 표시. 자신이 보낸 메시지도 이 알림으로 한 번만 표시 |
| LeaveResponse 성공 | RoomId와 참가자 목록 초기화, Lobby 전환 |
| 서버 오류 | 요청만 실패 처리하고 기존 상태 유지. 오류 내용 표시 |
| 연결 종료 | 대기 요청 전부 실패, 세션·방 정보 초기화, 연결 종료 표시 |

- 방 생성자에게 특별 권한은 없습니다. 생성자가 나가도 다른 참가자는 남습니다. 마지막 참가자가 나가면 방이 삭제됩니다.
- 채팅은 **UTF-8 기준 1~1024바이트**입니다. `Encoding.UTF8.GetByteCount(text)`로 검사합니다. C# `string.Length`는 이 제한과 다릅니다.
- 요청에는 채팅 텍스트만 보냅니다. 발신자와 방은 서버 세션에서 결정합니다.
- 일반 채팅은 같은 방 전체에 전달되며, 자신도 알림을 받습니다. UI에서 즉시 추가한다면 후속 알림과 중복되지 않게 처리해야 하므로 처음에는 서버 알림만 표시하는 방식을 권장합니다.
- C++ 클라이언트의 `/enter`, `/join`, `/say`는 콘솔 명령입니다. Unity에서는 문자열 명령을 보내지 않고 버튼에서 해당 Protobuf 요청을 생성합니다.

## 5. Unity 쪽 파일 구성 제안

다음 구성은 새 Unity 클라이언트의 제안이며 현재 서버 저장소에 C# 구현이 있는 것은 아닙니다.

| 파일/영역 | 책임 |
|---|---|
| Generated/ | protoc로 생성한 메시지. 수동 수정 금지 |
| ProtocolConstants.cs | 메시지 코드, 오류 코드, 헤더 오프셋과 최대 크기 |
| PacketHeader.cs / Packet.cs | 읽은 헤더와 소유한 본문 자료형 |
| PacketCodec.cs | big-endian 헤더 읽기·쓰기, 길이 검증 |
| TcpConnection.cs | 연결, 단일 수신 루프, 직렬 송신, 취소·종료 |
| PendingRequestRegistry.cs | requestId 발급, 예상 응답 코드, 타임아웃·실패 관리 |
| ServerMessageDispatcher.cs | 응답/알림 구분과 Protobuf 파싱 |
| PlayerSessionState.cs / RoomState.cs | 플레이어·방·참가자 로컬 상태 |
| RoomClientService.cs | Enter/Create/Join/Leave/Chat 요청 API |
| MainThreadEventPump.cs | 수신 이벤트를 Update에서 순서대로 적용 |
| LobbyView.cs / RoomView.cs / ChatView.cs | 버튼 입력과 화면 갱신 |

수신 처리 → FIFO 이벤트 큐 → `Update()`의 상태 변경 및 UI 반영 순서를 권장합니다. 큐 항목은 연결 세대와 소유 데이터로 구성합니다. 서버 응답과 알림을 별도 큐로 나누어 처리 순서가 뒤집히지 않게 합니다. Unity API 대부분은 스레드 안전하지 않으므로 수신 작업에서 GameObject·UI를 직접 변경하지 않습니다. [Unity 공식 스레드 안내](https://docs.unity3d.com/cn/2023.2/Manual/overview-of-dot-net-in-unity.html)

## 6. 송수신 구현 시 지켜야 할 것

1. **연결당 수신 루프 하나:** 버튼마다 Receive를 호출하지 않습니다. 헤더 8바이트를 끝까지 읽고 길이를 검증한 뒤 본문 `packetSize - 8`바이트를 끝까지 읽습니다. 한 번의 Read가 패킷 하나를 반환한다고 가정하지 않습니다.
2. **송신 직렬화:** 여러 요청의 바이트가 섞이지 않도록 단일 송신 큐 또는 잠금으로 패킷 전체 쓰기를 보호합니다. Socket.Send를 사용한다면 부분 송신도 처리합니다.
3. **전송 전 대기 등록:** requestId와 예상 응답 코드를 먼저 등록합니다. 전송 실패 시 등록한 요청도 실패로 정리합니다. ID는 재전송 중복 제거 키가 아닙니다.
4. **응답/알림 분리:** 0xC1~0xC3은 requestId=0이어야 합니다. 응답은 대기 요청 ID와 예상 코드를 검사합니다. 0xFF는 일반 예상 응답 코드와 다르더라도 서버 오류 경로로 처리합니다.
5. **일관된 적용 순서:** 같은 연결에서 받은 패킷 순서대로 세션·방 상태를 갱신합니다. Join 응답으로 참가자 목록을 적용하기 전에 다음 ChatMessage를 UI에 적용하지 않습니다. 요청 완료 콜백에도 적용된 최신 상태가 보이게 합니다.
6. **취소 가능한 종료:** 종료 토큰과 소켓 닫기로 수신 대기를 해제하고 작업 종료를 정리합니다. Unity 메인 스레드에서 `.Wait()`/`.Result`로 수신 작업을 무기한 기다리지 않습니다. OnDestroy/앱 종료 시에도 같은 종료 경로를 사용합니다.
7. **타임아웃 정책:** 방 생성·입장·퇴장·채팅의 타임아웃은 서버 처리 실패를 확정하지 않습니다. 자동 재전송하지 말고 연결을 종료·초기화하는 보수적 정책으로 시작합니다. 현재 서버에는 요청 중복 제거와 방 상태 재조회 API가 없습니다.
8. **재연결 세대:** 새 TCP 연결을 만들면 로컬 연결 세대를 증가시키고 이전 연결의 늦은 UI 이벤트·응답을 폐기합니다. 재연결은 새 Enter부터 시작하며 이전 PlayerId/RoomId 소속을 복구하지 않습니다.
9. **프로토콜 오류:** 잘못된 길이, 본문 파싱 실패, 알 수 없는 메시지, 알림의 잘못된 ID 등은 오류를 표시하고 연결을 정리합니다. 정상 서버 오류 응답과 구분합니다.

## 7. 오류 표시용 표

| 16진수 / 십진수 | 이름 | UI 안내 |
|---|---|---|
| 0x00 / 0 | None | 성공 |
| 0x01 / 1 | UnknownRequest | 지원하지 않는 요청 또는 잘못된 방향 |
| 0x02 / 2 | InvalidPayload | 메시지 형식·텍스트 길이 확인 |
| 0x03 / 3 | InvalidRequestStatus | requestId와 요청 errorCode 확인 |
| 0x11 / 17 | AlreadyEntered | 이미 개발용 입장을 완료함 |
| 0x12 / 18 | NotEntered | 먼저 Enter 요청 필요 |
| 0x13 / 19 | RoomNotFound | 해당 방이 없음. 방 ID 확인 |
| 0x14 / 20 | RoomFull | 방 정원 초과 |
| 0x15 / 21 | AlreadyInRoom | 기존 방에서 먼저 퇴장 필요 |
| 0x16 / 22 | NotInRoom | 먼저 방 생성 또는 입장 필요 |
| 0x17 / 23 | InvalidState | 현재 상태에서 사용할 수 없음 |
| 0x18 / 24 | InvalidCapacity | 정원은 1~16 |
| 0x99 / 153 | UnknownError | 서버 내부 오류 |

## 8. 구현 순서와 완료 확인

구현 순서: **헤더/Ping → Enter → 방 생성·입장 → 참가자 알림 → 채팅 → 종료·오류 처리**.

최소 UI: 서버 주소/포트, 연결 버튼, 내 플레이어 ID, 방 정원과 생성 버튼, 방 ID와 입장 버튼, 참가자 목록, 메시지 입력/전송 버튼, 퇴장 버튼, 오류 표시.

두 클라이언트 검증:

1. A와 B가 연결 후 각각 Enter 성공, 서로 다른 PlayerId 확인.
2. A가 capacity=2로 방 생성. 반환된 RoomId를 B가 사용해 입장.
3. B의 응답에는 A/B 모두 포함되고 A에는 PlayerJoined가 한 번 표시됨.
4. A/B가 한글·이모지 메시지를 주고받으며 발신자 포함 각 화면에 한 번씩 표시됨.
5. 다른 방에는 채팅·입퇴장 알림이 표시되지 않음. 세 번째 참가자는 RoomFull.
6. B의 명시적 퇴장 및 강제 종료 각각에서 A가 PlayerLeft를 한 번 수신.
7. 마지막 참가자 퇴장 후 기존 방 ID 입장은 RoomNotFound.
8. 빈 텍스트, UTF-8 1024/1025바이트, 중복 입장, 부분 수신·연속 패킷, 서버 종료·재연결을 확인.

최신 서버 실행 예시:

```powershell
.\out\build\refactor-debug\Debug\zpd-server.exe 20000
```

참조 클라이언트: `mockclient/src/RoomChatCommands.cpp`, `mockclient/src/RoomChatReceiver.cpp`. 정확한 스키마·상수의 기준은 `common/proto/*.proto`, `common/include/ProtocolLimits.hpp`, `common/include/MessageCode.hpp`, `common/include/ErrorCode.hpp`입니다. 상세 서버 규격은 [MO 통신 규격](MO_통신_규격.md)을 참고하세요.
