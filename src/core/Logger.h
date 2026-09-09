#pragma once

#include <iostream>
#include <string>
#include <string_view>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <mutex>
#include <deque>
#include <atomic>

enum class LogLevel : uint32_t {
    VulkanDebug = 0,
    Info,
    Warning,
    Error
};

struct LogEntry {
    LogLevel level;
    std::string timestamp;
    std::string message;
};

class Logger {
public:
    static Logger& Get() {
        static Logger instance;
        return instance;
    }

    void SetLogLevel(LogLevel level) noexcept {
        m_minLevel.store(level, std::memory_order_relaxed);
    }

    [[nodiscard]] LogLevel GetLogLevel() const noexcept {
        return m_minLevel.load(std::memory_order_relaxed);
    }

    void SetMaxLogs(size_t maxLogs) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_maxLogs = maxLogs;
        while (m_logs.size() > m_maxLogs && !m_logs.empty()) {
            m_logs.pop_front();
        }
    }

    [[nodiscard]] size_t GetMaxLogs() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_maxLogs;
    }

    void Log(LogLevel level, std::string_view msg) {
        if (static_cast<uint32_t>(level) < static_cast<uint32_t>(m_minLevel.load(std::memory_order_relaxed))) {
            return;
        }

        const auto now = std::chrono::system_clock::now();
        const auto in_time_t = std::chrono::system_clock::to_time_t(now);

        struct tm timeinfo{};
#if defined(_WIN32)
        localtime_s(&timeinfo, &in_time_t);
#else
        localtime_r(&in_time_t, &timeinfo);
#endif

        std::stringstream ss;
        ss << std::put_time(&timeinfo, "%H:%M:%S");
        std::string timeStr = ss.str();
        std::string messageStr(msg);

        const char* prefix = "[INFO] ";
        switch (level) {
            case LogLevel::VulkanDebug: prefix = "[VULKAN] "; break;
            case LogLevel::Info:        prefix = "[INFO] "; break;
            case LogLevel::Warning:     prefix = "[WARN] "; break;
            case LogLevel::Error:       prefix = "[ERROR] "; break;
        }

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_logs.size() >= m_maxLogs && m_maxLogs > 0) {
                m_logs.pop_front();
            }
            if (m_maxLogs > 0) {
                m_logs.push_back(LogEntry{ level, timeStr, std::move(messageStr) });
            }
        }

        std::cout << timeStr << " " << prefix << msg << std::endl;
    }

    [[nodiscard]] const std::deque<LogEntry>& GetLogs() const { return m_logs; }
    
    void ClearLogs() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_logs.clear();
    }

private:
    Logger() = default;
    mutable std::mutex m_mutex;
    std::deque<LogEntry> m_logs;
    size_t m_maxLogs = 2000;
    std::atomic<LogLevel> m_minLevel{LogLevel::Info};
};

#define LOG_INFO(msg) Logger::Get().Log(LogLevel::Info, msg)
#define LOG_WARN(msg) Logger::Get().Log(LogLevel::Warning, msg)
#define LOG_WARNING(msg) Logger::Get().Log(LogLevel::Warning, msg)
#define LOG_ERROR(msg) Logger::Get().Log(LogLevel::Error, msg)
#define LOG_VULKAN(msg) Logger::Get().Log(LogLevel::VulkanDebug, msg)

