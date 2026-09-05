#pragma once

#include <imgui.h>
#include <cstdint>
#include <string>
#include <vector>

namespace khepri {

enum class OracleEventType : uint8_t {
    None = 0,
    MouseMove = 1,
    MouseDown = 2,
    MouseUp = 3,
    MouseWheel = 4,
    KeyDown = 5,
    KeyUp = 6,
    Char = 7
};

#pragma pack(push, 1)
struct OracleInputPacket {
    uint8_t type = 0;     // OracleEventType
    uint8_t button = 0;   // 0=Left, 1=Middle, 2=Right
    uint16_t key = 0;     // ImGuiKey enum value or Unicode codepoint
    float x = 0.0f;       // Normalized [0..1]
    float y = 0.0f;       // Normalized [0..1]
    float wheel = 0.0f;   // Wheel delta (-1.0 or +1.0)
};
#pragma pack(pop)

class OracleBridge {
public:
    OracleBridge();
    ~OracleBridge();

    OracleBridge(const OracleBridge&) = delete;
    OracleBridge& operator=(const OracleBridge&) = delete;

    void PollEvents(ImGuiIO& io, int windowWidth, int windowHeight);
    bool IsConnected() const { return m_connected; }

private:
    void InitPipe();
    void ClosePipe();

    void* m_pipeHandle = nullptr; // HANDLE (void*)
    bool m_connected = false;
    bool m_hasOracleMouse = false;
    ImVec2 m_lastMousePos = ImVec2(-1.0f, -1.0f);
    std::vector<uint8_t> m_readBuffer;
};

} // namespace khepri
