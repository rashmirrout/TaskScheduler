#include <gtest/gtest.h>
#include "scheduler.h"
#include "sensor_task.h"
#include "actuator_task.h"
#include <thread>
#include <chrono>

using namespace task_scheduler;

class TaskLifecycleTest : public ::testing::Test {
protected:
    void SetUp() override {
        scheduler = std::make_unique<Scheduler>(2);
    }

    void TearDown() override {
        scheduler.reset();
    }

    std::unique_ptr<Scheduler> scheduler;
};

TEST_F(TaskLifecycleTest, CreateTask) {
    bool created = scheduler->createTask("TestTask", []() {
        return std::make_shared<SensorTask>(
            TaskConfig{"TestTask", 100, 10, 0, true, 10, 0, true}
        );
    });

    EXPECT_TRUE(created);
    EXPECT_EQ(scheduler->getTaskCount(), 1);
}

TEST_F(TaskLifecycleTest, CreateDuplicateTask) {
    scheduler->createTask("TestTask", []() {
        return std::make_shared<SensorTask>(
            TaskConfig{"TestTask", 100, 10, 0, true, 10, 0, true}
        );
    });

    // Try to create another task with same name
    bool created = scheduler->createTask("TestTask", []() {
        return std::make_shared<SensorTask>(
            TaskConfig{"TestTask", 100, 10, 0, true, 10, 0, true}
        );
    });

    EXPECT_FALSE(created);
    EXPECT_EQ(scheduler->getTaskCount(), 1);
}

TEST_F(TaskLifecycleTest, ScopePersistence) {
    // Create task in local scope
    {
        scheduler->createTask("ScopedTask", []() {
            return std::make_shared<SensorTask>(
                TaskConfig{"ScopedTask", 100, 10, 0, true, 10, 0, true}
            );
        });
    } // Scope ends, but task should persist in registry

    EXPECT_EQ(scheduler->getTaskCount(), 1);
    
    auto task = scheduler->getTask("ScopedTask");
    ASSERT_NE(task, nullptr);
    EXPECT_TRUE(task->isActive());
}

TEST_F(TaskLifecycleTest, StopTask) {
    scheduler->createTask("TestTask", []() {
        return std::make_shared<SensorTask>(
            TaskConfig{"TestTask", 100, 10, 0, true, 10, 0, true}
        );
    });

    EXPECT_EQ(scheduler->getTaskCount(), 1);

    bool stopped = scheduler->stopTask("TestTask");
    EXPECT_TRUE(stopped);
    EXPECT_EQ(scheduler->getTaskCount(), 0);
}

TEST_F(TaskLifecycleTest, StopNonexistentTask) {
    bool stopped = scheduler->stopTask("NonexistentTask");
    EXPECT_FALSE(stopped);
}

TEST_F(TaskLifecycleTest, GetTask) {
    scheduler->createTask("TestTask", []() {
        return std::make_shared<SensorTask>(
            TaskConfig{"TestTask", 100, 10, 0, true, 10, 0, true}
        );
    });

    auto task = scheduler->getTask("TestTask");
    ASSERT_NE(task, nullptr);
    EXPECT_EQ(task->getName(), "TestTask");
}

TEST_F(TaskLifecycleTest, GetNonexistentTask) {
    auto task = scheduler->getTask("NonexistentTask");
    EXPECT_EQ(task, nullptr);
}

TEST_F(TaskLifecycleTest, MultipleTasksLifecycle) {
    scheduler->createTask("Task1", []() {
        return std::make_shared<SensorTask>(
            TaskConfig{"Task1", 100, 10, 0, true, 10, 0, true}
        );
    });
    scheduler->createTask("Task2", []() {
        return std::make_shared<ActuatorTask>(
            TaskConfig{"Task2", 150, 10, 0, true, 10, 0, true}
        );
    });
    scheduler->createTask("Task3", []() {
        return std::make_shared<SensorTask>(
            TaskConfig{"Task3", 200, 10, 0, true, 10, 0, true}
        );
    });

    EXPECT_EQ(scheduler->getTaskCount(), 3);

    scheduler->stopTask("Task2");
    EXPECT_EQ(scheduler->getTaskCount(), 2);

    auto task1 = scheduler->getTask("Task1");
    auto task3 = scheduler->getTask("Task3");
    EXPECT_NE(task1, nullptr);
    EXPECT_NE(task3, nullptr);

    auto task2 = scheduler->getTask("Task2");
    EXPECT_EQ(task2, nullptr);
}

TEST_F(TaskLifecycleTest, TaskExecutesAfterCreation) {
    auto task = std::make_shared<ActuatorTask>(
        TaskConfig{"ExecutionTest", 50, 10, 0, true, 10, 0, true}
    );
    task->setCommand(true);

    scheduler->createTask("ExecutionTest", [task]() {
        return task;
    });

    // Wait for task to execute multiple times
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    // Task should have executed several times
    EXPECT_GT(task->getActionCount(), 0);
}
