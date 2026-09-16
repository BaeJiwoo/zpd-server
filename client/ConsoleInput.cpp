#include "ConsoleInput.hpp"
#include "RoomChatClient.hpp"
#include "ClientSettings.hpp"
#include <Windows.h>
#include <iostream>

void RunConsoleInput(RoomChatClient& client)
{
    const HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
    DWORD mode = 0;
    const bool console = GetConsoleMode(input, &mode) != 0;
    std::string line;
    std::wstring consoleLine;
    while (client.IsRunning()) {
        char ch = 0;
        if (console) {
            DWORD available = 0;
            if (!GetNumberOfConsoleInputEvents(input, &available))
                break;
            if (!available) {
                std::this_thread::sleep_for(ClientSettings::InputPollInterval);
                continue;
            }
            INPUT_RECORD record{};
            DWORD read = 0;
            if (!ReadConsoleInputW(input, &record, 1, &read))
                break;
            if (record.EventType != KEY_EVENT || !record.Event.KeyEvent.bKeyDown)
                continue;
            const auto wide = record.Event.KeyEvent.uChar.UnicodeChar;
            if (!wide)
                continue;
            if (wide == 3 || wide == 26)
                break;
            if (wide == L'\b') {
                if (!consoleLine.empty()) {
                    const auto last = consoleLine.back();
                    consoleLine.pop_back();
                    if (last >= 0xdc00 && last <= 0xdfff && !consoleLine.empty() &&
                        consoleLine.back() >= 0xd800 && consoleLine.back() <= 0xdbff)
                        consoleLine.pop_back();
                    std::cout << "\b \b" << std::flush;
                }
                continue;
            }
            if (wide != L'\r' && wide != L'\n') {
                consoleLine.append(record.Event.KeyEvent.wRepeatCount, wide);
                DWORD written = 0;
                const std::wstring echoed(record.Event.KeyEvent.wRepeatCount, wide);
                WriteConsoleW(GetStdHandle(STD_OUTPUT_HANDLE), echoed.data(),
                              static_cast<DWORD>(echoed.size()), &written, nullptr);
                continue;
            }
            const auto length = static_cast<int>(consoleLine.size());
            const int bytes = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, consoleLine.data(),
                                                  length, nullptr, 0, nullptr, nullptr);
            if (!consoleLine.empty() && bytes == 0) {
                std::cout << "Invalid Unicode input. Please retype.\n";
                consoleLine.clear();
                continue;
            }
            line.resize(bytes);
            if (bytes)
                WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, consoleLine.data(), length,
                                    line.data(), bytes, nullptr, nullptr);
            consoleLine.clear();
            ch = '\n';
            std::cout << '\n' << std::flush;
        } else {
            if (GetFileType(input) == FILE_TYPE_PIPE) {
                DWORD available = 0;
                if (!PeekNamedPipe(input, nullptr, 0, nullptr, &available, nullptr))
                    break;
                if (!available) {
                    std::this_thread::sleep_for(ClientSettings::InputPollInterval);
                    continue;
                }
            }
            DWORD read = 0;
            if (!ReadFile(input, &ch, 1, &read, nullptr) || read == 0) {
                if (!line.empty())
                    client.HandleInputLine(line);
                break;
            }
            if (ch == '\r')
                continue;
        }
        if (ch == '\r' || ch == '\n') {
            if (!client.HandleInputLine(line))
                break;
            line.clear();
        } else
            line += ch;
    }
}
