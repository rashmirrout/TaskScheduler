#include <gtest/gtest.h>
#include "scheduler.h"
#include "sensor_task.h"
#include "actuator_task.h"
#include <thread>
#include <chrono>
#include <vector>
#include <atomic>

using namespace task_scheduler;

class ConcurrencyTest : public ::testing::Test {
protected:
    void SetUp() override {
        scheduler = std::make_unique<Scheduler>(4);
    }

    void TearDown() override {
        scheduler.reset();
    }

    std::unique_ptr<Scheduler> scheduler;
};

TEST_F(ConcurrencyTest, MultipleTasksConcurrentExecution) {
    // Create multiple tasks
    for (int i = 0; i < 10; ++i) {
        std::string name = "Task" + std::to_string(i);
        scheduler->createTask(name, [name]() {
            return std::make_shared<SensorTask>(name, 50);
        });
    }

    EXPECT_EQ(scheduler->getTaskCount(), 10);

    // Let them run concurrently
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // All should still be active
    for (int i = 0; i < 10; ++i) {
        std::string name = "Task" + std::to_string(i);
        auto task = scheduler->getTask(name);
        ASSERT_NE(task, nullptr);
        EXPECT_TRUE(task->isActive());
    }
}

TEST_F(ConcurrencyTest, ConcurrentTaskCreation) {
    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};

    // Multiple threads creating tasks
    for (int i = 0; i < 5; ++i) {
        threads.emplace_back([this, i, &successCount]() {
            for (int j = 0; j < 4; ++j) {
                std::string name = "T" + std::to_string(i) + "_" + std::to_string(j);
                bool created = scheduler->createTask(name, [name]() {
                    return std::make_shared<SensorTask>(name, 100);
                });
                if (created) {
                    successCount++;
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(successCount.load(), 20);
    EXPECT_EQ(scheduler->getTaskCount(), 20);
}

TEST_F(ConcurrencyTest, ConcurrentStopTask) {
    // Create tasks
    for (int i = 0; i < 20; ++i) {
        std::string name = "Task" + std::to_string(i);
        scheduler->createTask(name, [name]() {
            return std::make_shared<SensorTask>(name, 100);
        });
    }

    std::vector<std::thread> threads;

    // Multiple threads stopping tasks
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back([this, i]() {
            for (int j = 0; j < 5; ++j) {
                std::string name = "Task" + std::to_string(i * 5 + j);
                scheduler->stopTask(name);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(scheduler->getTaskCount(), 0);
}

TEST_F(ConcurrencyTest, ConcurrentConfigUpdates) {
    auto sensor = std::make_shared<SensorTask>("TestTask", 50);
    sensor->setSensorValue(100.0);

    scheduler->createTask("TestTask", [sensor]() {
        return sensor;
    });

    std::vector<std::thread> threads;
    std::atomic<bool> running{true};

    // Thread continuously updating config
    threads.emplace_back([this, &running]() {
        while (running.load()) {
            scheduler->updateTask("TestTask", 50, 10, 0, true, 10, 0, true);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });

    // Let task run while config is being updated
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    running = false;
    for (auto& t : threads) {
        t.join();
    }

    // Task should still be valid
    auto task = scheduler->getTask("TestTask");
    ASSERT_NE(task, nullptr);
    EXPECT_TRUE(task->isActive());
}

TEST_F(ConcurrencyTest, MixedOperations) {
    std::vector<std::thread> threads;
    std::atomic<bool> running{true};

    // Thread 1: Creating tasks
    threads.emplace_back([this, &running]() {
        int count = 0;
        while (running.load() && count < 10) {
            std::string name = "Create" + std::to_string(count++);
            scheduler->createTask(name, [name]() {
                return std::make_shared<SensorTask>(name, 100);
            });
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    });

    // Thread 2: Updating tasks
    threads.emplace_back([this, &running]() {
        while (running.load()) {
            for (int i = 0; i < 5; ++i) {
                std::string name = "Create" + std::to_string(i);
                scheduler->updateTask(name, 150, 10, 0, true, 10, 0, true);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
        }
    });

    // Thread 3: Stopping tasks
    threads.emplace_back([this, &running]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        while (running.load()) {
            for (int i = 0; i < 3; ++i) {
                std::string name = "Create" + std::to_string(i);
                scheduler->stopTask(name);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    running = false;

    for (auto& t : threads) {
        t.join();
    }

    // Scheduler should still be in valid state
    EXPECT_GE(scheduler->getTaskCount(), 0);
}

TEST_F(ConcurrencyTest, StressTest) {
    std::vector<std::thread> threads;
    std::atomic<int> operationCount{0};

    for (int i = 0; i < 8; ++i) {
        threads.emplace_back([this, i, &operationCount]() {
            for (int j = 0; j < 50; ++j) {
                std::string name = "Stress" + std::to_string(i) + "_" + std::to_string(j);
                
                // Create
                scheduler->createTask(name, [name]() {
                    return std::make_shared<ActuatorTask>(name, 50);
                });
                operationCount++;

                // Update
                scheduler->updateTask(name, 100, 10, 0, true, 10, 0, true);
                operationCount++;

                // Stop
                scheduler->stopTask(name);
                operationCount++;
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // Should have performed all operations without crashes
    EXPECT_EQ(operationCount.load(), 8 * 50 * 3);
    EXPECT_EQ(scheduler->getTaskCount(), 0);  // All tasks stopped
}
