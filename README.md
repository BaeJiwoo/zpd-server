# zpd-server

A Windows C++20 MO room server using WinSock2, IOCP, Protobuf, CMake and vcpkg.

See [the MO requirements](docs/MO_서버_단계별_개발_요구사항.md) and [wire protocol](docs/MO_통신_규격.md).

## Requirements

- Visual Studio 2026 or Build Tools 2026 with Desktop development with C++ and the Windows SDK
- CMake 4.2 or later for the Visual Studio 2026 generator
- vcpkg with the `VCPKG_ROOT` environment variable set to its installation directory
- VS Code extensions: C/C++ (`ms-vscode.cpptools`) and CMake Tools (`ms-vscode.cmake-tools`)

This computer uses `C:\vcpkg`. Adjust the path for other installations. Restart VS Code after changing environment variables.

## Build and run

Run from the project root in PowerShell:

```powershell
$env:VCPKG_ROOT = 'C:\vcpkg'
cmake --preset windows-x64
cmake --build --preset debug
.\out\build\windows-x64\Debug\zpd-server.exe

cmake --build --preset release
.\out\build\windows-x64\Release\zpd-server.exe
```

In VS Code, select the `windows-x64` configure preset. Press Ctrl+Shift+B to build Debug or F5 to build and debug. Use Tasks: Run Task > CMake: Build Release for Release builds.

The server listens on all IPv4 interfaces at TCP port 20000 and accepts up to 1000 connections. Supply a port argument to override the default, such as `zpd-server.exe 9100`. Press Enter or close standard input to stop the server.

## Tests

Start the server first in one terminal:

```powershell
.\out\build\windows-x64\Debug\zpd-server.exe
```

Open a second terminal and run the interactive client:

```powershell
.\out\build\windows-x64\Debug\zpd-client.exe
```

Open two clients. Run `/enter` in both (development-only temporary identity, no authentication), then `/create 2` in the first. Run `/join <returned room ID>` in the second. Both show the room members; join/leave notifications arrive even while input is idle. Use `/leave`, `/members`, `/ping`, or `/quit`. Other text (or `/say <text>`) sends UTF-8 chat to everyone in the same room, including the sender. Chat is limited to 1024 UTF-8 bytes. `/echo <text>` retains the binary-preserving Echo check. Press Enter in the server terminal to stop it; idle clients also exit on disconnection.

Both programs default to port 20000. To use another port, pass the same port to both executables, such as `zpd-server.exe 9100` and `zpd-client.exe 9100`. The client connects to 127.0.0.1 and does not start an embedded server.

In VS Code, run Tasks: Run Task > Server: Run, then Tasks: Run Task > Client: Run Room Chat. The server task builds both programs. Each program uses its own terminal. The client task uses the existing build so the running server executable does not need to be rebuilt. For debugging, select Debug Room Chat Client after starting the server.

Automated integration checks remain available separately through CTest. They use `--no-pause`, start embedded servers on temporary ports, and display all logs:

```powershell
ctest --preset debug
ctest --preset release
```

Actual interactive-client regression checks (Python 3 standard library only) also exercise notifications while input is idle, shutdown, malformed responses and outstanding request cleanup:

```powershell
python mockclient/tests/chat_client_tests.py out/build/windows-x64/Debug
```

The dedicated interactive executable is `zpd-client.exe`. The existing `zpd-server-tests.exe <port>` entry point is retained for compatibility. See [code organization](docs/코드_구조.md) for module responsibilities and naming changes.

## Project structure

```text
root/
├── mockclient/
│   ├── include/
│   ├── src/
│   ├── tests/
│   └── CMakeLists.txt
├── common/
│   ├── include/
│   ├── proto/
│   ├── README.md
│   └── CMakeLists.txt
├── server/
│   ├── include/
│   ├── src/
│   ├── tests/
│   └── CMakeLists.txt
├── docs/
├── CMakeLists.txt
├── CMakePresets.json
└── vcpkg.json
```

- `mockclient`: the console room chat client and its process regression tests.
- `common`: Protobuf schemas, message/error codes, packet framing, protocol limits, shared C++ network defaults and serialization helpers. See [Unity sharing](common/README.md).
- `server`: IOCP transport, connection management, game/room/chat logic and server integration tests.
- `docs`: requirements, protocol specification and design notes.

Each module owns its CMake targets. The root configures shared build options and includes the modules. Headers are exposed through target dependencies; the mock client does not include server headers. Add implementation files to the owning module's `CMakeLists.txt`.

Executables remain under `out/build/windows-x64/Debug` or `Release`, so existing run commands and VS Code tasks continue to work. Run build and test commands from the repository root.

## Server behavior

The 8-byte big-endian packet header carries length, message code, error and request ID. Protobuf bodies are limited to 4088 bytes. One logic worker owns player sessions and rooms; disconnected players leave their room, and empty rooms are deleted. See the protocol document for message codes and errors.

Sends are serialized per connection. Each connection allows up to 256 queued sends of at most 4096 bytes each. A full queue or send failure disconnects that client. Pending I/O completions are drained before connection slots are reused or released. Peer FIN and server shutdown close the connection without guaranteeing delivery of queued responses.

Call `Start`, `Stop`, and `Port` from the owner thread. Callbacks are serialized per connection but may run concurrently across connections. Callbacks must not call `Stop` or allow exceptions to escape. Use `Send` and `Disconnect` from callbacks for the same connection. Client IDs are reusable slot numbers, not persistent session identifiers. Derived servers must call `Stop()` in their destructors while their callback implementations are still alive.

WinSock2 is provided by the Windows SDK. Protobuf is installed through the vcpkg manifest and generated during the build.

For Visual Studio 2022, change the preset generator to `Visual Studio 17 2022`, set the preset minimum CMake version to 3.25 or later, and use a separate build directory.

References: [vcpkg CMake integration](https://learn.microsoft.com/en-us/vcpkg/users/buildsystems/cmake-integration), [VS Code CMake presets](https://github.com/microsoft/vscode-cmake-tools/blob/main/docs/cmake-presets.md), [asynchronous socket closure](https://learn.microsoft.com/en-us/windows/win32/api/winsock/nf-winsock-closesocket).

