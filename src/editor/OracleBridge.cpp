#include "OracleBridge.h"
#include "../core/Logger.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace khepri {

OracleBridge::OracleBridge() {
    InitPipe();
}

OracleBridge::~OracleBridge() {
    ClosePipe();
}

void OracleBridge::InitPipe() {
#ifdef _WIN32
    if (m_pipeHandle && m_pipeHandle != INVALID_HANDLE_VALUE) {
        return;
    }

    // Create named pipe for non-blocking local IPC
    const wchar_t* pipeName = L"\\\\.\\pipe\\khepri_oracle_input";
    m_pipeHandle = CreateNamedPipeW(
        pipeName,
        PIPE_ACCESS_INBOUND,
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_NOWAIT,
        1,
        4096,
        4096,
        0,
        nullptr
    );

    if (m_pipeHandle == INVALID_HANDLE_VALUE) {
        m_pipeHandle = nullptr;
        LOG_WARN("OracleBridge: Failed to create named pipe '\\\\.\\pipe\\khepri_oracle_input'");
    } else {
        LOG_INFO("OracleBridge: Listening on named pipe '\\\\.\\pipe\\khepri_oracle_input'");
    }
#endif
}

void OracleBridge::ClosePipe() {
#ifdef _WIN32
    if (m_pipeHandle && m_pipeHandle != INVALID_HANDLE_VALUE) {
        FlushFileBuffers(m_pipeHandle);
        DisconnectNamedPipe(m_pipeHandle);
        CloseHandle(m_pipeHandle);
        m_pipeHandle = nullptr;
    }
    m_connected = false;
#endif
}

void OracleBridge::PollEvents(ImGuiIO& io, int windowWidth, int windowHeight) {
#ifdef _WIN32
    if (!m_pipeHandle || m_pipeHandle == INVALID_HANDLE_VALUE) {
        InitPipe();
        if (!m_pipeHandle) return;
    }

    HANDLE hPipe = (HANDLE)m_pipeHandle;

    DWORD bytesAvail = 0;
    BOOL peekOk = PeekNamedPipe(hPipe, nullptr, 0, nullptr, &bytesAvail, nullptr);
    if (!peekOk) {
        DWORD err = GetLastError();
        if (err == ERROR_PIPE_LISTENING) {
            ConnectNamedPipe(hPipe, nullptr);
        } else if (err == ERROR_BROKEN_PIPE || err == ERROR_PIPE_NOT_CONNECTED) {
            DisconnectNamedPipe(hPipe);
            ConnectNamedPipe(hPipe, nullptr);
            m_connected = false;
            m_readBuffer.clear();
        } else {
            ConnectNamedPipe(hPipe, nullptr);
        }
        return;
    }

    m_connected = true;
    // Ensure ImGui treats the window as focused when controlled via Oracle
    io.AddFocusEvent(true);

    if (bytesAvail > 0) {
        std::vector<uint8_t> temp(bytesAvail);
        DWORD bytesRead = 0;
        if (ReadFile(hPipe, temp.data(), bytesAvail, &bytesRead, nullptr) && bytesRead > 0) {
            m_readBuffer.insert(m_readBuffer.end(), temp.begin(), temp.begin() + bytesRead);
        }
    }

    // Process all full packets in buffer
    size_t packetSize = sizeof(OracleInputPacket);
    while (m_readBuffer.size() >= packetSize) {
        OracleInputPacket packet;
        memcpy(&packet, m_readBuffer.data(), packetSize);
        m_readBuffer.erase(m_readBuffer.begin(), m_readBuffer.begin() + packetSize);

        float px = packet.x * static_cast<float>(windowWidth);
        float py = packet.y * static_cast<float>(windowHeight);

        switch (static_cast<OracleEventType>(packet.type)) {
            case OracleEventType::MouseMove: {
                m_lastMousePos = ImVec2(px, py);
                m_hasOracleMouse = true;
                io.AddMousePosEvent(px, py);
                break;
            }
            case OracleEventType::MouseDown: {
                m_lastMousePos = ImVec2(px, py);
                m_hasOracleMouse = true;
                io.AddMousePosEvent(px, py);
                int btn = packet.button;
                if (btn >= 0 && btn < ImGuiMouseButton_COUNT) {
                    io.AddMouseButtonEvent(btn, true);
                }
                break;
            }
            case OracleEventType::MouseUp: {
                m_lastMousePos = ImVec2(px, py);
                m_hasOracleMouse = true;
                io.AddMousePosEvent(px, py);
                int btn = packet.button;
                if (btn >= 0 && btn < ImGuiMouseButton_COUNT) {
                    io.AddMouseButtonEvent(btn, false);
                }
                break;
            }
            case OracleEventType::MouseWheel: {
                io.AddMouseWheelEvent(0.0f, packet.wheel);
                break;
            }
            case OracleEventType::KeyDown: {
                ImGuiKey key = static_cast<ImGuiKey>(packet.key);
                if (key != ImGuiKey_None) {
                    io.AddKeyEvent(key, true);
                }
                break;
            }
            case OracleEventType::KeyUp: {
                ImGuiKey key = static_cast<ImGuiKey>(packet.key);
                if (key != ImGuiKey_None) {
                    io.AddKeyEvent(key, false);
                }
                break;
            }
            case OracleEventType::Char: {
                if (packet.key > 0) {
                    io.AddInputCharacter(packet.key);
                }
                break;
            }
            default:
                break;
        }
    }

    // Persist last oracle mouse position to prevent GLFW from resetting it to offscreen/physical coords
    if (m_hasOracleMouse) {
        io.AddMousePosEvent(m_lastMousePos.x, m_lastMousePos.y);
    }
#endif
}

} // namespace khepri
