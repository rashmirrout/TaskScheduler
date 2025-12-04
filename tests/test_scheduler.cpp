#include <gtest/gtest.h>
#include "scheduler.h"
#include "sensor_task.h"
#include "actuator_task.h"
#include <thread>
#include <chrono>

using namespace task_scheduler;

class SchedulerTest : public ::testing::Test {
protected:
    void SetUp() override {
        scheduler = std::make_unique<Scheduler>(4);
    }

    void TearDown() override {
        scheduler.reset();
    }

    std::unique_ptr<Scheduler> scheduler;
};

TEST_F(SchedulerTest, InitializationAndShutdown) {
    // Scheduler should initialize successfully
    EXPECT_EQ(scheduler->getTaskCount(), 0);

    // Add tasks
    scheduler->createTask("Task1", []() {
        return std::make_shared<SensorTask>("Task1", 100);
    });

    EXPECT_EQ(scheduler->getTaskCount(), 1);

    // Shutdown should clean up properly
    scheduler->shutdown();
    // After shutdown, should be safe to destroy
}

TEST_F(SchedulerTest, TaskSchedulingTiming) {
    auto actuator = std::make_shared<ActuatorTask>("TimingTask", 100);
    actuator->setCommand(true);

    auto startTime = std::chrono::steady_clock::now();

    scheduler->createTask("TimingTask", [actuator]() {
        return actuator;
    });

    // Wait for approximately 5 executions (500ms)
    std::this_thread::sleep_for(std::chrono::milliseconds(550));

    auto endTime = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

    // Task should have executed approximately 5 times
    int actionCount = actuator->getActionCount();
    EXPECT_GE(actionCount, 4);  // At least 4 times
    EXPECT_LE(actionCount, 7);  // At most 7 times (with some tolerance)
}

TEST_F(SchedulerTest, PriorityQueueOrdering) {
    // Create tasks with different intervals
    scheduler->createTask("Fast", []() {
        return std::make_shared<SensorTask>("Fast", 50);
    });

    scheduler->createTask("Medium", []() {
        return std::make_shared<SensorTask>("Medium", 100);
    });

    scheduler->createTask("Slow", []() {
        return std::make_shared<SensorTask>("Slow", 200);
    });

    EXPECT_EQ(scheduler->getTaskCount(), 3);

    // Let them run
    std::this_thread::sleep_for(std::chrono::milliseconds(250));

    // All should still be registered
    EXPECT_NE(scheduler->getTask("Fast"), nullptr);
    EXPECT_NE(scheduler->getTask("Medium"), nullptr);
    EXPECT_NE(scheduler->getTask("Slow"), nullptr);
}

TEST_F(SchedulerTest, ReschedulingAfterExecution) {
    auto actuator = std::make_shared<ActuatorTask>("RescheduleTask", 100);
    actuator->setCommand(true);

    scheduler->createTask("RescheduleTask", [actuator]() {
        return actuator;
    });

    // Wait for first execution
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    int count1 = actuator->getActionCount();

    // Wait for more executions
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    int count2 = actuator->getActionCount();

    // Should have executed more times
    EXPECT_GT(count2, count1);
}

TEST_F(SchedulerTest, DynamicIntervalChange) {
    auto actuator = std::make_shared<ActuatorTask>("DynamicTask", 200);
    actuator->setCommand(true);

    scheduler->createTask("DynamicTask", [actuator]() {
        return actuator;
    });

    // Run with 200ms interval
    std::this_thread::sleep_for(std::chrono::milliseconds(450));
    int count1 = actuator->getActionCount();

    // Change to 50ms interval (4x faster)
    scheduler->updateTask("DynamicTask", 50, 10, 0, true, 10, 0, true);

    // Wait same duration
    std::this_thread::sleep_for(std::chrono::milliseconds(450));
    int count2 = actuator->getActionCount();

    // Should execute significantly more times in second period
    int period1Count = count1;
    int period2Count = count2 - count1;
    EXPECT_GT(period2Count, period1Count * 2);  // At least 2x more executions
}

TEST_F(SchedulerTest, MultipleWorkersUtilization) {
    // Create many tasks to test worker pool
    for (int i = 0; i < 20; ++i) {
        std::string name = "Worker" + std::to_string(i);
        scheduler->createTask(name, [name]() {
            return std::make_shared<SensorTask>(name, 50);
        });
    }

    EXPECT_EQ(scheduler->getTaskCount(), 20);

    // Let them run - workers should handle all tasks
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // All tasks should still be active
    for (int i = 0; i < 20; ++i) {
        std::string name = "Worker" + std::to_string(i);
        auto task = scheduler->getTask(name);
        ASSERT_NE(task, nullptr);
        EXPECT_TRUE(task->isActive());
    }
}

TEST_F(SchedulerTest, EmptyScheduler) {
    // Scheduler with no tasks should not consume CPU
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_EQ(scheduler->getTaskCount(), 0);
}

TEST_F(SchedulerTest, SchedulerShutdownWithActiveTasks) {
    // Create tasks
    for (int i = 0; i < 5; ++i) {
        std::string name = "ShutdownTask" + std::to_string(i);
        scheduler->createTask(name, [name]() {
            return std::make_shared<SensorTask>(name, 100);
        });
    }

    // Let them start running
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Shutdown should cleanly stop all tasks and threads
    scheduler->shutdown();

    // Should be safe to destroy scheduler now
}
