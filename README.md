# zpd-server

A Windows C++20 TCP echo server using WinSock2, IOCP, CMake, vcpkg manifest mode, and VS Code.

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

The server listens on all IPv4 interfaces at TCP port 9000 and accepts up to 1000 connections. Supply a port argument to override the default, such as `zpd-server.exe 9100`. Press Enter or close standard input to stop the server.

## Tests

Start the server first in one terminal:

```powershell
.\out\build\windows-x64\Debug\zpd-server.exe
```

Open a second terminal and run the interactive client:

```powershell
.\out\build\windows-x64\Debug\zpd-server-tests.exe
```

Type a message and press Enter. The client displays the server reply as `Echo: your message`. The server terminal displays received and sent message contents. Type `/quit` to close the client. Press Enter in the server terminal to stop the server.

Both programs default to port 9000. To use another port, pass the same port to both executables, such as `zpd-server.exe 9100` and `zpd-server-tests.exe 9100`. The client connects to 127.0.0.1 and does not start an embedded server.

In VS Code, run Tasks: Run Task > Server: Run, then Tasks: Run Task > Test: Interactive Echo Client. The server task builds both programs. Each program uses its own terminal. The client task uses the existing build so the running server executable does not need to be rebuilt. For debugging, select Debug Interactive Echo Client after starting the server.

Automated integration checks remain available separately through CTest. They use `--no-pause`, start embedded servers on temporary ports, and display all logs:

```powershell
ctest --preset debug
ctest --preset release
```

## Project structure

- `include/zpd/ZPDServer.hpp`: echo callbacks and connection, send, and disconnect logs
- `include/zpd/IOCPServer.hpp`: connection acceptance, IOCP workers, and shutdown
- `include/zpd/ClientInfo.hpp`: per-connection sockets, send queues, and pending I/O tracking
- `include/zpd/Define.hpp`: shared buffer limits and I/O types
- `include/zpd/winsock.hpp` and `src/winsock.cpp`: standalone WinSock2 initialization check
- `src/main.cpp`: server startup and shutdown input
- `tests/echo_client.cpp`: interactive TCP echo client
- `tests/server_tests.cpp`: automated TCP integration tests
- `CMakeLists.txt`: targets, C++20 settings, include paths, and WinSock2 linking
- `CMakePresets.json`: Windows x64 configuration and Debug/Release build presets
- `vcpkg.json`: dependencies and baseline commit
- `.vscode/`: extension recommendations, IntelliSense, build tasks, and debugging

Place project headers under `include/` and include them using paths such as `#include "zpd/ZPDServer.hpp"`. CMake provides the same include path to the compiler and VS Code IntelliSense. Add new implementation files to the appropriate CMake target.

## Server behavior

The server echoes bytes unchanged, including null bytes and binary data. TCP receive boundaries are not message boundaries; no application packet protocol is defined.

Sends are serialized per connection. Each connection allows up to 256 queued sends of at most 4096 bytes each. A full queue or send failure disconnects that client. Pending I/O completions are drained before connection slots are reused or released. Peer FIN and server shutdown close the connection without guaranteeing delivery of queued responses.

Call `Start`, `Stop`, and `Port` from the owner thread. Callbacks are serialized per connection but may run concurrently across connections. Callbacks must not call `Stop` or allow exceptions to escape. Use `Send` and `Disconnect` from callbacks for the same connection. Client IDs are reusable slot numbers, not persistent session identifiers. Derived servers must call `Stop()` in their destructors while their callback implementations are still alive.

WinSock2 is provided by the Windows SDK, so the vcpkg dependency list is empty. Add external libraries to `vcpkg.json` and connect them with CMake `find_package` and `target_link_libraries`.

For Visual Studio 2022, change the preset generator to `Visual Studio 17 2022`, set the preset minimum CMake version to 3.25 or later, and use a separate build directory.

References: [vcpkg CMake integration](https://learn.microsoft.com/en-us/vcpkg/users/buildsystems/cmake-integration), [VS Code CMake presets](https://github.com/microsoft/vscode-cmake-tools/blob/main/docs/cmake-presets.md), [asynchronous socket closure](https://learn.microsoft.com/en-us/windows/win32/api/winsock/nf-winsock-closesocket).

