# 공통 통신 정의

`proto/`는 서버와 Unity 클라이언트가 함께 사용할 메시지 스키마입니다. `include/`는 서버와 C++ mockclient가 사용하는 코드이며, Unity에서 직접 사용하는 C# 라이브러리는 아닙니다.

| 경로 | 역할 |
|---|---|
| `proto/*.proto` | Echo, 입장, 방, 채팅 메시지의 필드와 번호 |
| `include/MessageCode.hpp`, `include/ErrorCode.hpp` | 패킷 헤더에 들어가는 메시지·오류 코드 |
| `include/ProtocolLimits.hpp` | 헤더 크기·오프셋과 본문·채팅·방 정원 한도 |
| `include/PacketHeader.hpp`, `include/Packet.hpp` | 8바이트 big-endian 헤더 읽기와 패킷 직렬화 |
| `include/ProtobufCodec.hpp` | C++ 본문 파싱·직렬화 |
| `include/NetworkSettings.hpp` | C++ 실행 프로그램의 기본 포트·버퍼·연결·송신 설정 |

## Unity 공유 권장 방식

같은 `.proto`에서 서버용 C++과 Unity용 C# 메시지를 각각 생성하는 방식을 권장합니다. 현재 CMake는 `proto/`에서 C++ 코드를 빌드 디렉터리로 생성합니다. C# 생성도 동일한 스키마를 입력으로 사용하면 필드 정의를 수동으로 복제할 필요가 없습니다.

저장소 루트에서 `protoc`가 PATH에 등록된 상태로 다음 명령을 실행할 수 있습니다.

```powershell
New-Item -ItemType Directory -Path out/generated/csharp -Force
protoc --proto_path=common/proto --csharp_out=out/generated/csharp common/proto/echo.proto common/proto/session.proto common/proto/room.proto common/proto/chat.proto
```

생성된 C# 파일은 `Google.Protobuf` 런타임을 참조해야 합니다. 서버와 Unity의 생성 도구 버전을 맞추고, 선택한 Unity 버전과 빌드 대상에서 런타임 호환성을 확인하세요. 생성 코드·런타임을 Unity 프로젝트에 연결하는 작업은 아직 포함하지 않았습니다. [Protobuf C# 생성 가이드](https://protobuf.dev/reference/csharp/csharp-generated/)

현재 `.proto`는 본문만 정의합니다. 메시지·오류 코드, 8바이트 헤더와 제한값은 C++ 헤더에 있으므로 위 명령만으로 C#에 생성되지 않습니다. Unity 연동 시 이 값들을 언어 중립적인 정의 파일로 옮기고 C++·C# 상수를 함께 생성하는 구성을 권장합니다. 헤더 읽기·쓰기는 각 언어로 구현하되 동일한 패킷 바이트를 사용하는 테스트로 일치 여부를 확인하면 됩니다. 상세 규격은 [MO 통신 규격](../docs/MO_통신_규격.md)을 참고하세요.

여러 Unity 프로젝트에서 재사용한다면 생성된 C# 메시지, 상수와 프레이밍 코드를 별도 UPM 패키지로 묶는 방식을 권장합니다. 저장소 내부에 패키지를 두어도 Unity Package Manager의 Git URL `?path=/하위폴더#태그` 형식으로 가져올 수 있습니다. 패키지에는 `package.json` 등 Unity 패키지 구성이 필요하며, 현재 `common` 자체는 UPM 패키지가 아닙니다. [Unity Git 패키지 의존성](https://docs.unity3d.com/6000.0/Documentation/Manual/upm-git.html)
