#pragma once

#include <iostream>
#include <string>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <mutex>
#include <vector>

enum class LogLevel {
    Info,
    Warning,
    Error,
    VulkanDebug
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

    void Log(LogLevel level, const std::string& msg) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        auto now = std::chrono::system_clock::now();
        auto in_time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&in_time_t), "%H:%M:%S");
        std::string timeStr = ss.str();

        LogEntry entry{ level, timeStr, msg };
        m_logs.push_back(entry);

        std::string prefix;
        switch (level) {
            case LogLevel::Info:        prefix = "[INFO] "; break;
            case LogLevel::Warning:     prefix = "[WARN] "; break;
            case LogLevel::Error:       prefix = "[ERROR] "; break;
            case LogLevel::VulkanDebug: prefix = "[VULKAN] "; break;
        }

        std::cout << timeStr << " " << prefix << msg << std::endl;
    }

    const std::vector<LogEntry>& GetLogs() const { return m_logs; }
    void ClearLogs() { std::lock_guard<std::mutex> lock(m_mutex); m_logs.clear(); }

private:
    Logger() = default;
    std::mutex m_mutex;
    std::vector<LogEntry> m_logs;
};

#define LOG_INFO(msg) Logger::Get().Log(LogLevel::Info, msg)
#define LOG_WARN(msg) Logger::Get().Log(LogLevel::Warning, msg)
#define LOG_ERROR(msg) Logger::Get().Log(LogLevel::Error, msg)
#define LOG_VULKAN(msg) Logger::Get().Log(LogLevel::VulkanDebug, msg)
