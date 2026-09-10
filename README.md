# zpd-server

Windows 전용 C++20 / WinSock2 프로젝트입니다. CMake와 vcpkg manifest 모드로 빌드하고 VS Code에서 개발합니다.

## 준비

- Visual Studio 2026 또는 Build Tools 2026: **C++를 사용한 데스크톱 개발** 및 Windows SDK
- CMake 4.2 이상 (기본 프리셋의 Visual Studio 2026 생성기에 필요)
- vcpkg 및 `VCPKG_ROOT` 환경 변수
- VS Code 확장: C/C++ (`ms-vscode.cpptools`), CMake Tools (`ms-vscode.cmake-tools`)

현재 PC의 vcpkg 경로는 `C:\vcpkg`입니다. 다른 환경에서는 설치 경로에 맞춰 `VCPKG_ROOT`를 설정하세요. 환경 변수를 변경한 후 VS Code를 다시 시작해야 합니다.

## 빌드 및 실행

프로젝트 루트의 PowerShell에서:

```powershell
$env:VCPKG_ROOT = 'C:\vcpkg'
cmake --preset windows-x64
cmake --build --preset debug
.\out\build\windows-x64\Debug\zpd-server.exe

cmake --build --preset release
.\out\build\windows-x64\Release\zpd-server.exe
```

VS Code에서 폴더를 열고 `windows-x64` 구성 프리셋을 선택하세요. `Ctrl+Shift+B`는 Debug 빌드, `F5`는 빌드 후 디버깅입니다. Release 빌드는 **Tasks: Run Task → CMake: Build Release**에서 실행할 수 있습니다.

## 프로젝트 구성

프로젝트 헤더는 `include/` 아래에 두고 `#include "zpd/winsock.hpp"`처럼 불러옵니다. CMake의 `target_include_directories`에 등록되어 빌드와 VS Code IntelliSense에서 같은 경로를 사용합니다. 새 구현 파일(`.cpp`)은 `CMakeLists.txt`의 `add_executable`에 추가하세요.

- `include/zpd/winsock.hpp`: WinSock2 확인 함수 선언
- `src/winsock.cpp`: WinSock2 초기화, TCP 소켓 생성 및 정리 구현
- `src/main.cpp`: 프로그램 진입점 및 WinSock2 확인 함수 호출
- `CMakeLists.txt`: C++20, MSVC 경고 옵션, `ws2_32` 링크
- `CMakePresets.json`: Windows x64 구성과 Debug/Release 빌드
- `vcpkg.json`: 의존성 목록 및 버전 기준 커밋
- `.vscode/`: 확장 추천, IntelliSense, 빌드 및 디버깅 설정

현재 실행 파일은 초기화 확인용이며 포트를 열거나 연결을 수락하지 않습니다. WinSock2는 Windows SDK에서 제공하므로 vcpkg 의존성은 비어 있습니다. 외부 라이브러리를 추가할 때 `vcpkg.json`의 `dependencies`에 등록하고 CMake에서 `find_package` 및 `target_link_libraries`로 연결하세요.

Visual Studio 2022를 사용할 경우 프리셋의 `generator`를 `Visual Studio 17 2022`로, `cmakeMinimumRequired`를 3.25 이상으로 변경하고 별도 빌드 디렉터리를 사용하세요.

참고: [vcpkg CMake 연동](https://learn.microsoft.com/en-us/vcpkg/users/buildsystems/cmake-integration), [VS Code CMake 프리셋](https://github.com/microsoft/vscode-cmake-tools/blob/main/docs/cmake-presets.md).
