# MO 통신 규격

2026-09-16 · 서버와 클라이언트는 이 규격으로 함께 빌드합니다. 이전 4바이트 헤더와 호환되지 않습니다.

## 헤더

| 오프셋 | 필드 | 크기 | 규칙 |
|---|---|---|---|
| 0 | 전체 길이 | uint16, 2바이트 | 헤더 포함, big-endian, 8~4096 |
| 2 | 메시지 코드 | uint8, 1바이트 | 아래 표 |
| 3 | 오류 코드 | uint8, 1바이트 | 요청·알림은 0 |
| 4 | requestId | uint32, 4바이트 | big-endian |

헤더는 8바이트, 최대 본문은 **4088바이트**입니다. 구조체 메모리 배치를 전송하지 않습니다.
요청 ID는 연결 내 미완료 요청과 중복되지 않는 0 이외의 값입니다. 클라이언트가 ID를 발급하고 대기 요청을 관리하며, 서버는 응답에 같은 ID를 복사합니다. 서버 알림은 ID 0입니다. 요청 ID는 재시도 중복 제거 또는 인증 토큰이 아닙니다.

오류 응답의 본문은 비웁니다. 알려진 요청에는 대응 응답 코드를 사용합니다. 알 수 없는 코드나 클라이언트가 보낸 응답·알림 코드는 ErrorResponse로 거절합니다. 잘못된 패킷 길이는 연결을 종료합니다. Protobuf 필드 번호를 변경·재사용하지 않으며 삭제 시 `reserved`로 남깁니다.

## 메시지

본문 타입은 `protocol` 네임스페이스의 `proto/echo.proto`, `session.proto`, `room.proto`, `chat.proto` 메시지입니다.

| 요청 코드·타입 | 응답 코드·타입 | 본문 |
|---|---|---|
| 0x01 EchoRequest | 0x81 EchoResponse | bytes data; 바이너리 보존 |
| 0x02 PingRequest | 0x82 PingResponse | 빈 본문 |
| 0x03 EnterRequest | 0x83 EnterResponse | 요청 없음 → 임시 player_id |
| 0x04 CreateRoomRequest | 0x84 CreateRoomResponse | capacity → room_id, capacity, player_ids |
| 0x05 JoinRoomRequest | 0x85 JoinRoomResponse | room_id → room_id, capacity, player_ids |
| 0x06 LeaveRoomRequest | 0x86 LeaveRoomResponse | 요청 없음 → 떠난 room_id |
| 0x07 ChatRequest | 0x87 ChatResponse | UTF-8 text → 빈 성공 응답 |
| — | 0xc1 PlayerJoined | room_id, player_id; 서버 알림 |
| — | 0xc2 PlayerLeft | room_id, player_id; 서버 알림 |
| — | 0xc3 ChatMessage | room_id, player_id, text; 서버 알림 |
| 잘못된 방향·알 수 없는 코드 | 0xff ErrorResponse | 빈 본문; 원래 requestId |

입장 전 Echo, Ping, Enter만 허용합니다. Enter는 실제 인증 없이 임시 플레이어 ID를 발급하는 개발용 동작입니다.
방 정원은 `ProtocolLimits::MinRoomCapacity=1`부터 `ProtocolLimits::MaxRoomCapacity=16`까지입니다. 생성자는 자동 참가합니다. 방 상태는 Waiting이며 참가자 목록은 본인을 포함합니다. 한 플레이어는 한 방에만 속합니다. 생성자에게 특별한 권한은 없습니다.
입장 알림은 기존 참가자에게, 퇴장 알림은 남은 참가자에게만 전송합니다. 명시적 퇴장과 연결 종료는 공통 정리 경로를 사용합니다. 빈 방은 즉시 삭제합니다. 재접속 복구는 없습니다.

## 오류

| 값 | 이름 | 의미 |
|---|---|---|
| 0x00 | None | 성공 |
| 0x01 | UnknownRequest | 알 수 없는 코드·잘못된 방향 |
| 0x02 | InvalidPayload | 본문 파싱 실패·부적절한 본문 |
| 0x03 | InvalidRequestStatus | 요청 오류 필드가 0이 아니거나 requestId가 0 |
| 0x11 | AlreadyEntered | 개발용 입장 중복 |
| 0x12 | NotEntered | 입장 전 방 요청 |
| 0x13 | RoomNotFound | 없는 방 |
| 0x14 | RoomFull | 정원 초과 |
| 0x15 | AlreadyInRoom | 이미 방에 속함 |
| 0x16 | NotInRoom | 소속 방 없이 퇴장 요청 |
| 0x17 | InvalidState | 허용되지 않는 플레이어·방 상태 |
| 0x18 | InvalidCapacity | 정원 설정 범위 위반 |
| 0x99 | UnknownError | 내부 직렬화 실패·ID 소진 |

메시지 처리 중 예외나 송신 실패는 해당 연결을 종료해 게임 상태를 정리합니다. 한 참가자에게 송신할 수 없어도 나머지 참가자 전송은 계속합니다. 네트워크 전송 직전에 연결 세대를 재검사합니다.

## 큐와 종료

이벤트 큐는 최대 1024자리이며 살아 있는 연결마다 종료 이벤트 자리 하나를 예약합니다. 연결 생성은 생성 이벤트와 종료 예약 자리 둘이 필요합니다. 일반 요청 포화는 해당 연결을 종료합니다. 연결별 송신 큐는 최대 256개입니다.
게임 상태는 로직 스레드 하나만 변경합니다. 종료는 요청 접수 중단 → 로직 스레드 종료 및 대기 이벤트·게임 상태 폐기 → 네트워크 I/O 정리 순서입니다. 종료 시 알림 전달 완료를 기다리지 않습니다. ID는 실행 내에서만 유효하며 재시작 시 1부터 시작합니다.

## 방 채팅 확장

참가자는 방에 입장한 뒤 일반 텍스트 또는 `/say 내용`으로 메시지를 보냅니다. `/echo 내용`은 자신에게만 반환하는 통신 점검 명령입니다. ChatRequest에는 text만 있고, 방과 발신자 ID는 서버 세션에서 결정합니다. 입장 전에는 NotEntered, 소속 방이 없으면 NotInRoom으로 거절합니다.

채팅은 UTF-8 기준 1~1024바이트이며 빈 본문, 초과 길이, 잘못된 UTF-8·Protobuf는 InvalidPayload입니다. 클라이언트는 긴 메시지를 임의로 나누지 않고 길이 오류를 안내합니다. 서버는 요청 ID를 유지한 빈 ChatResponse를 보낸 뒤 같은 방 참가자 전원(발신자 포함)에게 ID 0 ChatMessage를 전달합니다. 한 로직 스레드에서 처리한 순서대로 전달하며 저장·재전송 기능은 없습니다. 다른 방과 로비에는 전달하지 않습니다.
