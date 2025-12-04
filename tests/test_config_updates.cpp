#include <gtest/gtest.h>
#include "scheduler.h"
#include "sensor_task.h"
#include <thread>
#include <chrono>

using namespace task_scheduler;

class ConfigUpdatesTest : public ::testing::Test {
protected:
    void SetUp() override {
        scheduler = std::make_unique<Scheduler>(2);
    }

    void TearDown() override {
        scheduler.reset();
    }

    std::unique_ptr<Scheduler> scheduler;
};

TEST_F(ConfigUpdatesTest, UpdateInterval) {
    scheduler->createTask("TestTask", []() {
        return std::make_shared<SensorTask>("TestTask", 1000);
    });

    auto task = scheduler->getTask("TestTask");
    EXPECT_EQ(task->getInterval(), 1000);

    // Update interval
    scheduler->updateTask("TestTask", 500, 10, 0, true, 10, 0, true);
    EXPECT_EQ(task->getInterval(), 500);
}

TEST_F(ConfigUpdatesTest, UpdateNonexistentTask) {
    bool updated = scheduler->updateTask("NonexistentTask", 500, 10, 0, true, 10, 0, true);
    EXPECT_FALSE(updated);
}

TEST_F(ConfigUpdatesTest, UpdateTolerance) {
    auto sensor = std::make_shared<SensorTask>("TestTask", 100);
    sensor->setSensorValue(100.0);  // Above threshold

    scheduler->createTask("TestTask", [sensor]() {
        return sensor;
    });

    // Wait for some executions
    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    // Update to higher tolerance
    scheduler->updateTask("TestTask", 100, 20, 0, true, 20, 0, true);

    // Task should continue running with new tolerance
    auto task = scheduler->getTask("TestTask");
    ASSERT_NE(task, nullptr);
    EXPECT_TRUE(task->isActive());
}

TEST_F(ConfigUpdatesTest, UpdateGates) {
    auto sensor = std::make_shared<SensorTask>("TestTask", 100);
    sensor->setSensorValue(100.0);

    scheduler->createTask("TestTask", [sensor]() {
        return sensor;
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    // Close signal gate
    scheduler->updateTask("TestTask", 100, 10, 0, false, 10, 0, true);

    auto task = scheduler->getTask("TestTask");
    ASSERT_NE(task, nullptr);

    // Re-open gate
    scheduler->updateTask("TestTask", 100, 10, 0, true, 10, 0, true);
}

TEST_F(ConfigUpdatesTest, UpdateRepeat) {
    auto sensor = std::make_shared<SensorTask>("TestTask", 50);
    sensor->setSensorValue(100.0);

    scheduler->createTask("TestTask", [sensor]() {
        return sensor;
    });

    // Start with no repeat
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Enable repeat (heartbeat)
    scheduler->updateTask("TestTask", 50, 5, 3, true, 5, 0, true);

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    auto task = scheduler->getTask("TestTask");
    EXPECT_TRUE(task->isActive());
}

TEST_F(ConfigUpdatesTest, ConcurrentUpdates) {
    scheduler->createTask("TestTask", []() {
        return std::make_shared<SensorTask>("TestTask", 100);
    });

    // Multiple threads updating config
    std::vector<std::thread> threads;
    for (int i = 0; i < 5; ++i) {
        threads.emplace_back([this, i]() {
            int interval = 50 + i * 10;
            for (int j = 0; j < 10; ++j) {
                scheduler->updateTask("TestTask", interval, 10, 0, true, 10, 0, true);
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // Task should still be valid after concurrent updates
    auto task = scheduler->getTask("TestTask");
    ASSERT_NE(task, nullptr);
    EXPECT_TRUE(task->isActive());
}

TEST_F(ConfigUpdatesTest, UpdateWhileRunning) {
    auto sensor = std::make_shared<SensorTask>("TestTask", 50);
    sensor->setSensorValue(100.0);

    scheduler->createTask("TestTask", [sensor]() {
        return sensor;
    });

    // Let it run for a bit
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Update config while task is actively running
    for (int i = 0; i < 20; ++i) {
        scheduler->updateTask("TestTask", 50 + i, 10, 0, true, 10, 0, true);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    auto task = scheduler->getTask("TestTask");
    EXPECT_TRUE(task->isActive());
}
