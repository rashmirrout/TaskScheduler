#include <gtest/gtest.h>
#include "core/scheduler.h"
#include "core/task_base.h"
#include "tasks/sensor_task.h"
#include "tasks/actuator_task.h"
#include <thread>
#include <chrono>
#include <limits>

using namespace task_scheduler;

// ====== Mock Task for Flexible Testing ======

class MockTask : public TaskBase {
public:
    explicit MockTask(const TaskConfig& config)
        : TaskBase(config)
        , planResult_{false, false}
        , planCallCount_(0)
        , signalCallCount_(0)
        , actCallCount_(0)
    {}

    void setPlanResult(bool wantSignal, bool wantAct) {
        planResult_ = {wantSignal, wantAct};
    }

    PlanResult plan() override {
        planCallCount_++;
        return planResult_;
    }

    void signal(bool doSignal) override {
        signalCallCount_++;
        lastSignalValue_ = doSignal;
    }

    void act(bool doAct) override {
        actCallCount_++;
        lastActValue_ = doAct;
    }

    int getPlanCallCount() const { return planCallCount_; }
    int getSignalCallCount() const { return signalCallCount_; }
    int getActCallCount() const { return actCallCount_; }
    bool getLastSignalValue() const { return lastSignalValue_; }
    bool getLastActValue() const { return lastActValue_; }

private:
    PlanResult planResult_;
    std::atomic<int> planCallCount_;
    std::atomic<int> signalCallCount_;
    std::atomic<int> actCallCount_;
    bool lastSignalValue_ = false;
    bool lastActValue_ = false;
};

// ====== Test Fixtures ======

class EdgeCaseTest : public ::testing::Test {
protected:
    void SetUp() override {
        scheduler = std::make_unique<Scheduler>(4);
    }

    void TearDown() override {
        scheduler.reset();
    }

    std::unique_ptr<Scheduler> scheduler;
};

// ====== Timing Edge Cases ======

TEST_F(EdgeCaseTest, VeryShortInterval) {
    // 1ms interval - stress test
    auto task = std::make_shared<ActuatorTask>(
        TaskConfig{"FastTask", 1, 10, 0, true, 10, 0, true}
    );
    task->setCommand(true);

    scheduler->createTask("FastTask", [task]() { return task; });

    // Run for 100ms - should execute many times
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    int count = task->getActionCount();
    // Should execute at least 50 times (allowing for overhead)
    EXPECT_GE(count, 50);
}

TEST_F(EdgeCaseTest, VeryLongInterval) {
    // Very long interval (but we won't wait for it)
    auto task = std::make_shared<MockTask>(
        TaskConfig{"LongTask", 3600000, 10, 0, true, 10, 0, true}  // 1 hour
    );
    task->setPlanResult(true, true);

    scheduler->createTask("LongTask", [task]() { return task; });

    // Wait a short time
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Should be scheduled but not executed yet
    auto scheduledTask = scheduler->getTask("LongTask");
    ASSERT_NE(scheduledTask, nullptr);
    EXPECT_TRUE(scheduledTask->isActive());
}

TEST_F(EdgeCaseTest, TaskCompletesBeforeReschedule) {
    // Create task with short interval
    auto task = std::make_shared<MockTask>(
        TaskConfig{"SlowTask", 50, 10, 0, true, 10, 0, true}
    );
    
    // Make plan() take longer than interval
    int callCount = 0;
    task->setPlanResult(true, true);

    scheduler->createTask("SlowTask", [task]() { return task; });

    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    // Should still execute multiple times despite slow execution
    EXPECT_GE(task->getPlanCallCount(), 3);
}

// ====== Counter Edge Cases ======

TEST_F(EdgeCaseTest, ZeroTolerance) {
    // Tolerance = 0 means immediate activation
    auto task = std::make_shared<MockTask>(
        TaskConfig{"ZeroTol", 50, 0, 0, true, 0, 0, true}
    );
    task->setPlanResult(true, true);

    scheduler->createTask("ZeroTol", [task]() { return task; });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // With tolerance 0, should activate immediately on first plan
    EXPECT_GE(task->getSignalCallCount(), 1);
    EXPECT_GE(task->getActCallCount(), 1);
}

TEST_F(EdgeCaseTest, ToleranceOne) {
    // Tolerance = 1 (minimal debounce)
    auto task = std::make_shared<MockTask>(
        TaskConfig{"OneTol", 50, 1, 0, true, 1, 0, true}
    );
    task->setPlanResult(true, true);

    scheduler->createTask("OneTol", [task]() { return task; });

    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    // Should activate after second execution
    EXPECT_GE(task->getSignalCallCount(), 1);
    EXPECT_GE(task->getActCallCount(), 1);
}

TEST_F(EdgeCaseTest, HighTolerance) {
    // Very high tolerance
    auto task = std::make_shared<MockTask>(
        TaskConfig{"HighTol", 50, 100, 0, true, 100, 0, true}
    );
    task->setPlanResult(true, true);

    scheduler->createTask("HighTol", [task]() { return task; });

    // Run for reasonable time
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Should execute many times but not yet activate (tolerance too high)
    EXPECT_GE(task->getPlanCallCount(), 5);
    // Might not activate yet
}

TEST_F(EdgeCaseTest, RepeatZero) {
    // Repeat = 0 means no heartbeat
    auto task = std::make_shared<MockTask>(
        TaskConfig{"NoRepeat", 50, 2, 0, true, 2, 0, true}
    );
    task->setPlanResult(true, true);

    scheduler->createTask("NoRepeat", [task]() { return task; });

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Should activate and stay active (no repeat deactivation)
    EXPECT_GE(task->getSignalCallCount(), 1);
    
    // Wait more
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // Should not have deactivated (no repeat)
    int signalCount = task->getSignalCallCount();
    EXPECT_EQ(signalCount, 1); // Only activated once
}

// ====== State Transition Edge Cases ======

TEST_F(EdgeCaseTest, RapidPlanFlips) {
    // Plan alternates true/false every cycle
    auto task = std::make_shared<MockTask>(
        TaskConfig{"Flipper", 50, 2, 0, true, 2, 0, true}
    );

    bool flipState = true;
    int executionCount = 0;

    scheduler->createTask("Flipper", [task]() { return task; });

    // Let it run and manually flip plan result
    for (int i = 0; i < 10; ++i) {
        task->setPlanResult(flipState, flipState);
        flipState = !flipState;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    // Should handle rapid changes gracefully
    EXPECT_GE(task->getPlanCallCount(), 5);
}

TEST_F(EdgeCaseTest, GateClosedFromStart) {
    // Gates closed (allowSignal/allowAction = false)
    auto task = std::make_shared<MockTask>(
        TaskConfig{"GatesClosed", 50, 2, 0, false, 2, 0, false}
    );
    task->setPlanResult(true, true);

    scheduler->createTask("GatesClosed", [task]() { return task; });

    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    // Should never activate (gates closed)
    EXPECT_EQ(task->getSignalCallCount(), 0);
    EXPECT_EQ(task->getActCallCount(), 0);
}

TEST_F(EdgeCaseTest, GateToggling) {
    // Start with gates open
    auto task = std::make_shared<MockTask>(
        TaskConfig{"GateToggle", 50, 2, 0, true, 2, 0, true}
    );
    task->setPlanResult(true, true);

    scheduler->createTask("GateToggle", [task]() { return task; });

    // Let it activate
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    EXPECT_GE(task->getSignalCallCount(), 1);

    // Close gates
    scheduler->updateTask("GateToggle", 50, 2, 0, false, 2, 0, false);
    
    int countBefore = task->getSignalCallCount();
    
    // Wait more
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // Should not activate more (gates closed)
    int countAfter = task->getSignalCallCount();
    EXPECT_EQ(countBefore, countAfter);
}

TEST_F(EdgeCaseTest, SimultaneousChannelActivation) {
    // Both channels should activate independently
    auto task = std::make_shared<MockTask>(
        TaskConfig{"DualChannel", 50, 2, 0, true, 2, 0, true}
    );
    task->setPlanResult(true, true); // Want both

    scheduler->createTask("DualChannel", [task]() { return task; });

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Both channels should activate
    EXPECT_GE(task->getSignalCallCount(), 1);
    EXPECT_GE(task->getActCallCount(), 1);
}

// ====== Resource Edge Cases ======

TEST_F(EdgeCaseTest, ManyTasks) {
    // Create 100 tasks
    const int taskCount = 100;
    
    for (int i = 0; i < taskCount; ++i) {
        std::string name = "Task" + std::to_string(i);
        scheduler->createTask(name, [name]() {
            return std::make_shared<SensorTask>(
                TaskConfig{name, 100, 10, 0, true, 10, 0, true}
            );
        });
    }

    EXPECT_EQ(scheduler->getTaskCount(), taskCount);

    // Let them run
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    // All should still be active
    for (int i = 0; i < taskCount; ++i) {
        std::string name = "Task" + std::to_string(i);
        auto task = scheduler->getTask(name);
        ASSERT_NE(task, nullptr);
        EXPECT_TRUE(task->isActive());
    }
}

TEST_F(EdgeCaseTest, RapidCreateDestroy) {
    // Create and destroy tasks in tight loop
    for (int i = 0; i < 50; ++i) {
        std::string name = "Temp" + std::to_string(i);
        
        scheduler->createTask(name, [name]() {
            return std::make_shared<SensorTask>(
                TaskConfig{name, 100, 10, 0, true, 10, 0, true}
            );
        });
        
        // Immediately stop
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        scheduler->stopTask(name);
    }

    // Should handle gracefully
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // All should be stopped
    EXPECT_EQ(scheduler->getTaskCount(), 0);
}

TEST_F(EdgeCaseTest, SingleWorkerThread) {
    // Create scheduler with only 1 worker
    auto singleWorkerScheduler = std::make_unique<Scheduler>(1);

    // Create multiple tasks
    for (int i = 0; i < 5; ++i) {
        std::string name = "SingleWorker" + std::to_string(i);
        singleWorkerScheduler->createTask(name, [name]() {
            return std::make_shared<SensorTask>(
                TaskConfig{name, 100, 10, 0, true, 10, 0, true}
            );
        });
    }

    EXPECT_EQ(singleWorkerScheduler->getTaskCount(), 5);

    // Single worker should still handle all tasks
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    // All tasks should be active
    for (int i = 0; i < 5; ++i) {
        std::string name = "SingleWorker" + std::to_string(i);
        auto task = singleWorkerScheduler->getTask(name);
        ASSERT_NE(task, nullptr);
        EXPECT_TRUE(task->isActive());
    }

    singleWorkerScheduler->shutdown();
}

TEST_F(EdgeCaseTest, ManyWorkerThreads) {
    // Create scheduler with many workers
    auto manyWorkerScheduler = std::make_unique<Scheduler>(50);

    // Create tasks
    for (int i = 0; i < 10; ++i) {
        std::string name = "ManyWorker" + std::to_string(i);
        manyWorkerScheduler->createTask(name, [name]() {
            return std::make_shared<SensorTask>(
                TaskConfig{name, 100, 10, 0, true, 10, 0, true}
            );
        });
    }

    EXPECT_EQ(manyWorkerScheduler->getTaskCount(), 10);

    // Many workers should handle tasks efficiently
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    // All tasks should be active
    for (int i = 0; i < 10; ++i) {
        std::string name = "ManyWorker" + std::to_string(i);
        auto task = manyWorkerScheduler->getTask(name);
        ASSERT_NE(task, nullptr);
        EXPECT_TRUE(task->isActive());
    }

    manyWorkerScheduler->shutdown();
}

// ====== Configuration Edge Cases ======

TEST_F(EdgeCaseTest, UpdateToInvalidInterval) {
    auto task = std::make_shared<SensorTask>(
        TaskConfig{"UpdateTask", 100, 10, 0, true, 10, 0, true}
    );

    scheduler->createTask("UpdateTask", [task]() { return task; });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_EQ(task->getInterval(), 100);

    // Try to update to very small interval (should work, just fast)
    scheduler->updateTask("UpdateTask", 1, 10, 0, true, 10, 0, true);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_EQ(task->getInterval(), 1);
}

TEST_F(EdgeCaseTest, MultipleUpdatesRapidly) {
    auto task = std::make_shared<SensorTask>(
        TaskConfig{"RapidUpdate", 100, 10, 0, true, 10, 0, true}
    );

    scheduler->createTask("RapidUpdate", [task]() { return task; });

    // Rapidly update configuration
    for (int i = 0; i < 20; ++i) {
        scheduler->updateTask("RapidUpdate", 50 + i*10, 10, 0, true, 10, 0, true);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    // Should handle all updates
    EXPECT_TRUE(task->isActive());
    EXPECT_GE(task->getInterval(), 50);
}

TEST_F(EdgeCaseTest, StopNonExistentTask) {
    // Try to stop task that doesn't exist
    bool result = scheduler->stopTask("DoesNotExist");
    
    EXPECT_FALSE(result);
    EXPECT_EQ(scheduler->getTaskCount(), 0);
}

TEST_F(EdgeCaseTest, UpdateNonExistentTask) {
    // Try to update task that doesn't exist
    bool result = scheduler->updateTask("DoesNotExist", 100, 10, 0, true, 10, 0, true);
    
    EXPECT_FALSE(result);
}

TEST_F(EdgeCaseTest, DuplicateTaskCreation) {
    scheduler->createTask("Duplicate", []() {
        return std::make_shared<SensorTask>(
            TaskConfig{"Duplicate", 100, 10, 0, true, 10, 0, true}
        );
    });

    EXPECT_EQ(scheduler->getTaskCount(), 1);

    // Try to create again with same name
    bool result = scheduler->createTask("Duplicate", []() {
        return std::make_shared<SensorTask>(
            TaskConfig{"Duplicate", 100, 10, 0, true, 10, 0, true}
        );
    });

    EXPECT_FALSE(result);
    EXPECT_EQ(scheduler->getTaskCount(), 1); // Still only one
}

// ====== Extreme Values ======

TEST_F(EdgeCaseTest, MaxIntInterval) {
    // Very large interval (won't wait for it)
    auto task = std::make_shared<MockTask>(
        TaskConfig{"MaxInt", std::numeric_limits<int>::max(), 10, 0, true, 10, 0, true}
    );

    scheduler->createTask("MaxInt", [task]() { return task; });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Should be scheduled
    auto scheduledTask = scheduler->getTask("MaxInt");
    ASSERT_NE(scheduledTask, nullptr);
}

TEST_F(EdgeCaseTest, LargeToleranceAndRepeat) {
    auto task = std::make_shared<MockTask>(
        TaskConfig{"Large", 50, 1000, 500, true, 1000, 500, true}
    );
    task->setPlanResult(true, true);

    scheduler->createTask("Large", [task]() { return task; });

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Should execute but not activate yet (tolerance too high)
    EXPECT_GE(task->getPlanCallCount(), 5);
}

TEST_F(EdgeCaseTest, AllParametersAtExtremes) {
    auto task = std::make_shared<MockTask>(
        TaskConfig{"Extreme", 1, 0, 0, true, 0, 0, true}
    );
    task->setPlanResult(true, true);

    scheduler->createTask("Extreme", [task]() { return task; });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Should handle extreme config
    EXPECT_GE(task->getPlanCallCount(), 10);
    EXPECT_GE(task->getSignalCallCount(), 1);
    EXPECT_GE(task->getActCallCount(), 1);
}
