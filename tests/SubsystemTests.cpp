#include <gtest/gtest.h>
#include "core/ISubsystem.h"
#include "core/SubsystemManager.h"
#include "core/EngineContext.h"
#include <vector>
#include <string>

namespace {

// Test mock subsystem that records lifecycle event timestamps/orders
class MockSubsystem : public khepri::ISubsystem {
public:
    MockSubsystem(const std::string& name, std::vector<std::string>& eventLog)
        : m_name(name), m_log(eventLog) {}

    void Initialize([[maybe_unused]] khepri::EngineContext& context) override {
        m_initialized = true;
        m_log.push_back(m_name + "::Initialize");
    }

    void Update([[maybe_unused]] float deltaTime) override {
        m_log.push_back(m_name + "::Update");
    }

    void RenderUI() override {
        m_log.push_back(m_name + "::RenderUI");
    }

    void Shutdown() override {
        m_shutdown = true;
        m_log.push_back(m_name + "::Shutdown");
    }

    [[nodiscard]] const char* GetName() const noexcept override {
        return m_name.c_str();
    }

    bool IsInitialized() const noexcept { return m_initialized; }
    bool IsShutdown() const noexcept { return m_shutdown; }

private:
    std::string m_name;
    std::vector<std::string>& m_log;
    bool m_initialized = false;
    bool m_shutdown = false;
};

class SubsystemA : public MockSubsystem {
public:
    explicit SubsystemA(std::vector<std::string>& log) : MockSubsystem("SubsystemA", log) {}
};

class SubsystemB : public MockSubsystem {
public:
    explicit SubsystemB(std::vector<std::string>& log) : MockSubsystem("SubsystemB", log) {}
};

class SubsystemC : public MockSubsystem {
public:
    explicit SubsystemC(std::vector<std::string>& log) : MockSubsystem("SubsystemC", log) {}
};

TEST(SubsystemManagerTest, SubsystemRegistrationAndTypeLookup) {
    khepri::SubsystemManager manager;
    std::vector<std::string> log;

    auto* subA = manager.AddSubsystem<SubsystemA>(log);
    auto* subB = manager.AddSubsystem<SubsystemB>(log);

    ASSERT_NE(subA, nullptr);
    ASSERT_NE(subB, nullptr);
    EXPECT_EQ(manager.GetSubsystemCount(), 2u);

    EXPECT_EQ(manager.GetSubsystem<SubsystemA>(), subA);
    EXPECT_EQ(manager.GetSubsystem<SubsystemB>(), subB);
    EXPECT_EQ(manager.GetSubsystem<SubsystemC>(), nullptr);
}

TEST(SubsystemManagerTest, ForwardInitializationAndLIFOShutdownOrdering) {
    khepri::SubsystemManager manager;
    std::vector<std::string> log;

    manager.AddSubsystem<SubsystemA>(log);
    manager.AddSubsystem<SubsystemB>(log);
    manager.AddSubsystem<SubsystemC>(log);

    khepri::EngineContext dummyCtx{};
    manager.InitializeAll(dummyCtx);
    EXPECT_TRUE(manager.IsInitialized());

    // Verify forward initialization order: A -> B -> C
    ASSERT_EQ(log.size(), 3u);
    EXPECT_EQ(log[0], "SubsystemA::Initialize");
    EXPECT_EQ(log[1], "SubsystemB::Initialize");
    EXPECT_EQ(log[2], "SubsystemC::Initialize");

    log.clear();
    manager.UpdateAll(0.016f);
    ASSERT_EQ(log.size(), 3u);
    EXPECT_EQ(log[0], "SubsystemA::Update");
    EXPECT_EQ(log[1], "SubsystemB::Update");
    EXPECT_EQ(log[2], "SubsystemC::Update");

    log.clear();
    manager.RenderUIAll();
    ASSERT_EQ(log.size(), 3u);
    EXPECT_EQ(log[0], "SubsystemA::RenderUI");
    EXPECT_EQ(log[1], "SubsystemB::RenderUI");
    EXPECT_EQ(log[2], "SubsystemC::RenderUI");

    log.clear();
    // Shutdown: must execute in strict reverse (LIFO) order: C -> B -> A
    manager.ShutdownAll();
    EXPECT_FALSE(manager.IsInitialized());
    EXPECT_EQ(manager.GetSubsystemCount(), 0u);

    ASSERT_EQ(log.size(), 3u);
    EXPECT_EQ(log[0], "SubsystemC::Shutdown");
    EXPECT_EQ(log[1], "SubsystemB::Shutdown");
    EXPECT_EQ(log[2], "SubsystemA::Shutdown");
}

TEST(SubsystemManagerTest, DestructorAutomaticallyShutsDownLIFO) {
    std::vector<std::string> log;
    {
        khepri::SubsystemManager manager;
        manager.AddSubsystem<SubsystemA>(log);
        manager.AddSubsystem<SubsystemB>(log);

        khepri::EngineContext dummyCtx{};
        manager.InitializeAll(dummyCtx);
        log.clear();
    }
    // Destructor of manager should have invoked ShutdownAll in LIFO order
    ASSERT_EQ(log.size(), 2u);
    EXPECT_EQ(log[0], "SubsystemB::Shutdown");
    EXPECT_EQ(log[1], "SubsystemA::Shutdown");
}

TEST(SubsystemManagerTest, DoubleShutdownIsIdempotent) {
    khepri::SubsystemManager manager;
    std::vector<std::string> log;
    manager.AddSubsystem<SubsystemA>(log);

    khepri::EngineContext dummyCtx{};
    manager.InitializeAll(dummyCtx);

    log.clear();
    manager.ShutdownAll();
    EXPECT_EQ(log.size(), 1u);
    EXPECT_EQ(log[0], "SubsystemA::Shutdown");

    // Second shutdown must not crash and must do nothing
    EXPECT_NO_THROW(manager.ShutdownAll());
    EXPECT_EQ(log.size(), 1u);
}

} // namespace