#include <gtest/gtest.h>
#include "scheduler.h"
#include "sensor_task.h"
#include "actuator_task.h"
#include <thread>
#include <chrono>

using namespace task_scheduler;

class LazyDeletionTest : public ::testing::Test {
protected:
    void SetUp() override {
        scheduler = std::make_unique<Scheduler>(2);
    }

    void TearDown() override {
        scheduler.reset();
    }

    std::unique_ptr<Scheduler> scheduler;
};

TEST_F(LazyDeletionTest, TaskStoppedImmediately) {
    auto actuator = std::make_shared<ActuatorTask>("StopTest", 100);
    actuator->setCommand(true);

    scheduler->createTask("StopTest", [actuator]() {
        return actuator;
    });

    // Let it execute a few times
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
    int countBefore = actuator->getActionCount();
    EXPECT_GT(countBefore, 0);

    // Stop the task
    scheduler->stopTask("StopTest");

    // Wait - should not execute anymore
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    int countAfter = actuator->getActionCount();

    // Count should not increase (or increase by at most 1 if it was already scheduled)
    EXPECT_LE(countAfter, countBefore + 1);
}

TEST_F(LazyDeletionTest, TaskRemovedFromRegistry) {
    scheduler->createTask("RemoveTest", []() {
        return std::make_shared<SensorTask>("RemoveTest", 100);
    });

    EXPECT_EQ(scheduler->getTaskCount(), 1);

    // Stop task
    scheduler->stopTask("RemoveTest");

    // Should be removed from registry immediately
    EXPECT_EQ(scheduler->getTaskCount(), 0);

    // Getting task should return nullptr
    auto task = scheduler->getTask("RemoveTest");
    EXPECT_EQ(task, nullptr);
}

TEST_F(LazyDeletionTest, InactiveTaskDroppedFromQueue) {
    auto actuator = std::make_shared<ActuatorTask>("QueueTest", 50);
    actuator->setCommand(true);

    scheduler->createTask("QueueTest", [actuator]() {
        return actuator;
    });

    // Let it run
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Stop it
    scheduler->stopTask("QueueTest");

    // Even though task might still be in priority queue,
    // it should be dropped when popped (lazy deletion)
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Task should no longer be in registry
    EXPECT_EQ(scheduler->getTaskCount(), 0);
}

TEST_F(LazyDeletionTest, MultipleTasksPartialDeletion) {
    // Create multiple tasks
    for (int i = 0; i < 5; ++i) {
        std::string name = "Task" + std::to_string(i);
        scheduler->createTask(name, [name]() {
            return std::make_shared<SensorTask>(name, 100);
        });
    }

    EXPECT_EQ(scheduler->getTaskCount(), 5);

    // Stop some tasks
    scheduler->stopTask("Task1");
    scheduler->stopTask("Task3");

    EXPECT_EQ(scheduler->getTaskCount(), 3);

    // Remaining tasks should still be active
    EXPECT_NE(scheduler->getTask("Task0"), nullptr);
    EXPECT_NE(scheduler->getTask("Task2"), nullptr);
    EXPECT_NE(scheduler->getTask("Task4"), nullptr);

    // Stopped tasks should be gone
    EXPECT_EQ(scheduler->getTask("Task1"), nullptr);
    EXPECT_EQ(scheduler->getTask("Task3"), nullptr);
}

TEST_F(LazyDeletionTest, StopAndRecreate) {
    scheduler->createTask("RecreateTest", []() {
        return std::make_shared<SensorTask>("RecreateTest", 100);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Stop task
    scheduler->stopTask("RecreateTest");
    EXPECT_EQ(scheduler->getTaskCount(), 0);

    // Should be able to recreate with same name
    bool created = scheduler->createTask("RecreateTest", []() {
        return std::make_shared<SensorTask>("RecreateTest", 100);
    });

    EXPECT_TRUE(created);
    EXPECT_EQ(scheduler->getTaskCount(), 1);
}

TEST_F(LazyDeletionTest, TaskInactiveCheckInRun) {
    auto actuator = std::make_shared<ActuatorTask>("InactiveTest", 100);
    actuator->setCommand(true);

    scheduler->createTask("InactiveTest", [actuator]() {
        return actuator;
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    int countBefore = actuator->getActionCount();

    // Mark task inactive
    auto task = scheduler->getTask("InactiveTest");
    task->setActive(false);

    // Task's run() method should check active flag and return early
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    int countAfter = actuator->getActionCount();

    // Should not execute anymore
    EXPECT_EQ(countAfter, countBefore);
}

TEST_F(LazyDeletionTest, NoMemoryLeakAfterStop) {
    // Create tasks with shared_ptr
    std::weak_ptr<SensorTask> weakRef;

    {
        auto sensor = std::make_shared<SensorTask>("LeakTest", 100);
        weakRef = sensor;

        scheduler->createTask("LeakTest", [sensor]() {
            return sensor;
        });

        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        // Stop task
        scheduler->stopTask("LeakTest");
    }

    // Give time for cleanup
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Weak pointer should expire (task should be deleted)
    EXPECT_TRUE(weakRef.expired());
}

TEST_F(LazyDeletionTest, StopAllTasks) {
    // Create multiple tasks
    std::vector<std::string> taskNames;
    for (int i = 0; i < 10; ++i) {
        std::string name = "StopAll" + std::to_string(i);
        taskNames.push_back(name);
        scheduler->createTask(name, [name]() {
            return std::make_shared<SensorTask>(name, 100);
        });
    }

    EXPECT_EQ(scheduler->getTaskCount(), 10);

    // Stop all tasks
    for (const auto& name : taskNames) {
        scheduler->stopTask(name);
    }

    EXPECT_EQ(scheduler->getTaskCount(), 0);

    // All should be gone
    for (const auto& name : taskNames) {
        EXPECT_EQ(scheduler->getTask(name), nullptr);
    }
}
