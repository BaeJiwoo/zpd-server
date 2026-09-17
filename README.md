# zpd-server

A Windows C++20 packet server skeleton using WinSock2, IOCP, Protobuf, CMake and vcpkg.

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

```powershell
ctest --preset debug
ctest --preset release
```

The server tests cover packet framing, fragmented and coalesced input, connection reuse,
handler restart, valid packets without responses, and disconnection on malformed frames.

The mock room client and shared protocol schemas are retained as references. The server
currently has no Echo, Ping, entry, room, chat, position, or game-command handlers, so the
interactive service commands and old client/server regression scenarios cannot succeed.

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
- `server`: IOCP transport, connection management, packet framing, a basic event worker and server tests.
- `docs`: requirements, protocol specification and design notes.

Each module owns its CMake targets. The root configures shared build options and includes the modules. Headers are exposed through target dependencies; the mock client does not include server headers. Add implementation files to the owning module's `CMakeLists.txt`.

Executables remain under `out/build/windows-x64/Debug` or `Release`, so existing run commands and VS Code tasks continue to work. Run build and test commands from the repository root.

## Server behavior

The 8-byte big-endian packet header carries length, message code, error and request ID. Protobuf bodies are limited to 4088 bytes. One logic worker waits for connected, packet-received and disconnected events and removes them from the queue. Its event handling branches are intentionally empty. No service state, periodic ticks, responses or broadcasts are generated. Protocol and service design documents describe reference behavior, not currently implemented services.

Sends are serialized per connection. Each connection allows up to 256 queued sends of at most 4096 bytes each. A full queue or send failure disconnects that client. Pending I/O completions are drained before connection slots are reused or released. Peer FIN and server shutdown close the connection without guaranteeing delivery of queued responses.

Call `Start`, `Stop`, and `Port` from the owner thread. Callbacks are serialized per connection but may run concurrently across connections. Callbacks must not call `Stop` or allow exceptions to escape. Use `Send` and `Disconnect` from callbacks for the same connection. Client IDs are reusable slot numbers, not persistent session identifiers. Derived servers must call `Stop()` in their destructors while their callback implementations are still alive.

WinSock2 is provided by the Windows SDK. Protobuf is installed through the vcpkg manifest and generated during the build.

For Visual Studio 2022, change the preset generator to `Visual Studio 17 2022`, set the preset minimum CMake version to 3.25 or later, and use a separate build directory.

References: [vcpkg CMake integration](https://learn.microsoft.com/en-us/vcpkg/users/buildsystems/cmake-integration), [VS Code CMake presets](https://github.com/microsoft/vscode-cmake-tools/blob/main/docs/cmake-presets.md), [asynchronous socket closure](https://learn.microsoft.com/en-us/windows/win32/api/winsock/nf-winsock-closesocket).
