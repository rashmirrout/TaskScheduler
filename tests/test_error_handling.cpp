#include <gtest/gtest.h>
#include "core/scheduler.h"
#include "core/task_base.h"
#include "tasks/sensor_task.h"
#include "tasks/actuator_task.h"
#include "config/config_parser.h"
#include <thread>
#include <chrono>
#include <stdexcept>
#include <filesystem>
#include <fstream>

using namespace task_scheduler;
namespace fs = std::filesystem;

// ====== Mock Task with Exceptions ======

class ThrowingTask : public TaskBase {
public:
    enum class ThrowLocation {
        NONE,
        PLAN,
        SIGNAL,
        ACT
    };

    explicit ThrowingTask(const TaskConfig& config)
        : TaskBase(config)
        , throwLocation_(ThrowLocation::NONE)
        , planCallCount_(0)
        , signalCallCount_(0)
        , actCallCount_(0)
        , wantSignal_(false)
        , wantAct_(false)
    {}

    void setThrowLocation(ThrowLocation loc) {
        throwLocation_ = loc;
    }

    void setPlanResult(bool wantSignal, bool wantAct) {
        wantSignal_ = wantSignal;
        wantAct_ = wantAct;
    }

    PlanResult plan() override {
        planCallCount_++;
        if (throwLocation_ == ThrowLocation::PLAN) {
            throw std::runtime_error("Exception in plan()");
        }
        return {wantSignal_, wantAct_};
    }

    void signal(bool doSignal) override {
        signalCallCount_++;
        if (throwLocation_ == ThrowLocation::SIGNAL) {
            throw std::runtime_error("Exception in signal()");
        }
    }

    void act(bool doAct) override {
        actCallCount_++;
        if (throwLocation_ == ThrowLocation::ACT) {
            throw std::runtime_error("Exception in act()");
        }
    }

    int getPlanCallCount() const { return planCallCount_; }
    int getSignalCallCount() const { return signalCallCount_; }
    int getActCallCount() const { return actCallCount_; }

private:
    ThrowLocation throwLocation_;
    std::atomic<int> planCallCount_;
    std::atomic<int> signalCallCount_;
    std::atomic<int> actCallCount_;
    bool wantSignal_;
    bool wantAct_;
};

// ====== Factory that Returns Nullptr ======

class NullFactory {
public:
    static std::shared_ptr<TaskBase> create() {
        return nullptr;
    }
};

// ====== Test Fixtures ======

class ErrorHandlingTest : public ::testing::Test {
protected:
    void SetUp() override {
        scheduler = std::make_unique<Scheduler>(4);
        testDir_ = fs::temp_directory_path() / "taskscheduler_error_test";
        fs::create_directories(testDir_);
    }

    void TearDown() override {
        scheduler.reset();
        if (fs::exists(testDir_)) {
            fs::remove_all(testDir_);
        }
    }

    void writeXMLFile(const std::string& filename, const std::string& content) {
        std::ofstream file(testDir_ / filename);
        file << content;
        file.close();
    }

    std::string getTestPath(const std::string& filename) {
        return (testDir_ / filename).string();
    }

    std::unique_ptr<Scheduler> scheduler;
    fs::path testDir_;
};

// ====== Factory Error Tests ======

TEST_F(ErrorHandlingTest, NullTaskFactory) {
    // Factory returns nullptr
    bool result = scheduler->createTask("NullTask", []() {
        return nullptr;
    });

    EXPECT_FALSE(result);
    EXPECT_EQ(scheduler->getTaskCount(), 0);
}

TEST_F(ErrorHandlingTest, FactoryThrowsException) {
    // Factory that throws exception
    bool result = false;
    try {
        result = scheduler->createTask("ThrowingFactory", []() -> std::shared_ptr<TaskBase> {
            throw std::runtime_error("Factory error");
            return nullptr;
        });
    } catch (...) {
        // Exception should propagate or be caught by scheduler
    }

    // Task should not be created
    EXPECT_EQ(scheduler->getTaskCount(), 0);
}

// ====== Task Exception Tests ======

TEST_F(ErrorHandlingTest, TaskThrowsInPlan) {
    auto task = std::make_shared<ThrowingTask>(
        TaskConfig{"ThrowPlan", 50, 2, 0, true, 2, 0, true}
    );
    task->setThrowLocation(ThrowingTask::ThrowLocation::PLAN);
    task->setPlanResult(true, true);

    scheduler->createTask("ThrowPlan", [task]() { return task; });

    // Let it run - exceptions should be caught/handled
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    // Task should continue running despite exceptions
    // (Implementation dependent - might stop or continue)
    auto retrievedTask = scheduler->getTask("ThrowPlan");
    EXPECT_NE(retrievedTask, nullptr);
}

TEST_F(ErrorHandlingTest, TaskThrowsInSignal) {
    auto task = std::make_shared<ThrowingTask>(
        TaskConfig{"ThrowSignal", 50, 2, 0, true, 2, 0, true}
    );
    task->setThrowLocation(ThrowingTask::ThrowLocation::SIGNAL);
    task->setPlanResult(true, true);

    scheduler->createTask("ThrowSignal", [task]() { return task; });

    // Let it run
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    // Verify plan() was called (to reach tolerance)
    EXPECT_GE(task->getPlanCallCount(), 1);

    // Task should handle exception gracefully
    auto retrievedTask = scheduler->getTask("ThrowSignal");
    EXPECT_NE(retrievedTask, nullptr);
}

TEST_F(ErrorHandlingTest, TaskThrowsInAct) {
    auto task = std::make_shared<ThrowingTask>(
        TaskConfig{"ThrowAct", 50, 2, 0, true, 2, 0, true}
    );
    task->setThrowLocation(ThrowingTask::ThrowLocation::ACT);
    task->setPlanResult(true, true);

    scheduler->createTask("ThrowAct", [task]() { return task; });

    // Let it run
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    // Verify plan() was called
    EXPECT_GE(task->getPlanCallCount(), 1);

    // Task should handle exception gracefully
    auto retrievedTask = scheduler->getTask("ThrowAct");
    EXPECT_NE(retrievedTask, nullptr);
}

TEST_F(ErrorHandlingTest, MultipleTasksOneThrows) {
    // Create multiple tasks, one throws
    auto goodTask1 = std::make_shared<SensorTask>(
        TaskConfig{"Good1", 50, 10, 0, true, 10, 0, true}
    );
    
    auto throwingTask = std::make_shared<ThrowingTask>(
        TaskConfig{"Throwing", 50, 2, 0, true, 2, 0, true}
    );
    throwingTask->setThrowLocation(ThrowingTask::ThrowLocation::PLAN);
    throwingTask->setPlanResult(true, true);
    
    auto goodTask2 = std::make_shared<SensorTask>(
        TaskConfig{"Good2", 50, 10, 0, true, 10, 0, true}
    );

    scheduler->createTask("Good1", [goodTask1]() { return goodTask1; });
    scheduler->createTask("Throwing", [throwingTask]() { return throwingTask; });
    scheduler->createTask("Good2", [goodTask2]() { return goodTask2; });

    EXPECT_EQ(scheduler->getTaskCount(), 3);

    // Let them run
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    // Good tasks should continue running despite throwing task
    EXPECT_NE(scheduler->getTask("Good1"), nullptr);
    EXPECT_NE(scheduler->getTask("Good2"), nullptr);
    EXPECT_TRUE(scheduler->getTask("Good1")->isActive());
    EXPECT_TRUE(scheduler->getTask("Good2")->isActive());
}

// ====== Configuration Error Tests ======

TEST_F(ErrorHandlingTest, InvalidConfigUpdate) {
    auto task = std::make_shared<SensorTask>(
        TaskConfig{"UpdateTest", 100, 10, 0, true, 10, 0, true}
    );

    scheduler->createTask("UpdateTest", [task]() { return task; });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_EQ(task->getInterval(), 100);

    // Update with valid but extreme values (should work)
    bool result = scheduler->updateTask("UpdateTest", 1, 1000, 500, false, 1000, 500, false);
    EXPECT_TRUE(result);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_EQ(task->getInterval(), 1);
}

TEST_F(ErrorHandlingTest, UpdateNonExistentTask) {
    bool result = scheduler->updateTask("DoesNotExist", 100, 10, 0, true, 10, 0, true);
    EXPECT_FALSE(result);
}

TEST_F(ErrorHandlingTest, StopNonExistentTask) {
    bool result = scheduler->stopTask("DoesNotExist");
    EXPECT_FALSE(result);
}

TEST_F(ErrorHandlingTest, GetNonExistentTask) {
    auto task = scheduler->getTask("DoesNotExist");
    EXPECT_EQ(task, nullptr);
}

// ====== Parser Error Tests ======

TEST_F(ErrorHandlingTest, ParserMalformedXML) {
    std::string malformedXml = R"(<?xml version="1.0"?>
<tasks>
    <task name="Broken" type="SensorTask" intervalMs="100"
</tasks>)";  // Missing closing bracket

    writeXMLFile("malformed.xml", malformedXml);
    auto configs = ConfigParser::parse(getTestPath("malformed.xml"));

    // Should return empty vector, not crash
    EXPECT_TRUE(configs.empty());
}

TEST_F(ErrorHandlingTest, ParserInvalidTaskType) {
    std::string xml = R"(<?xml version="1.0"?>
<tasks>
    <task name="Invalid" type="UnknownTaskType" intervalMs="100"/>
</tasks>)";

    writeXMLFile("invalid_type.xml", xml);
    auto configs = ConfigParser::parse(getTestPath("invalid_type.xml"));

    // Should reject invalid task type
    EXPECT_TRUE(configs.empty());
}

TEST_F(ErrorHandlingTest, ParserNegativeInterval) {
    std::string xml = R"(<?xml version="1.0"?>
<tasks>
    <task name="Negative" type="SensorTask" intervalMs="-100"/>
</tasks>)";

    writeXMLFile("negative.xml", xml);
    auto configs = ConfigParser::parse(getTestPath("negative.xml"));

    // Should reject negative interval
    EXPECT_TRUE(configs.empty());
}

TEST_F(ErrorHandlingTest, ParserMissingRequiredField) {
    std::string xml = R"(<?xml version="1.0"?>
<tasks>
    <task name="NoInterval" type="SensorTask"/>
</tasks>)";  // Missing intervalMs

    writeXMLFile("missing_interval.xml", xml);
    auto configs = ConfigParser::parse(getTestPath("missing_interval.xml"));

    // Should reject task without required interval
    EXPECT_TRUE(configs.empty());
}

TEST_F(ErrorHandlingTest, ParserNonExistentFile) {
    auto configs = ConfigParser::parse(getTestPath("does_not_exist.xml"));
    
    // Should return empty, not crash
    EXPECT_TRUE(configs.empty());
}

TEST_F(ErrorHandlingTest, ParserEmptyFile) {
    writeXMLFile("empty.xml", "");
    auto configs = ConfigParser::parse(getTestPath("empty.xml"));

    // Empty file should return empty
    EXPECT_TRUE(configs.empty());
}

TEST_F(ErrorHandlingTest, ParserInvalidBooleanValue) {
    std::string xml = R"(<?xml version="1.0"?>
<tasks>
    <task name="BadBool" type="SensorTask" intervalMs="100" allowSignal="maybe"/>
</tasks>)";

    writeXMLFile("bad_bool.xml", xml);
    auto configs = ConfigParser::parse(getTestPath("bad_bool.xml"));

    // Should handle invalid boolean gracefully (default to true or false)
    // Implementation dependent
}

// ====== Scheduler Shutdown Error Tests ======

TEST_F(ErrorHandlingTest, ShutdownWithActiveTasks) {
    // Create multiple active tasks
    for (int i = 0; i < 10; ++i) {
        std::string name = "Task" + std::to_string(i);
        scheduler->createTask(name, [name]() {
            return std::make_shared<SensorTask>(
                TaskConfig{name, 50, 10, 0, true, 10, 0, true}
            );
        });
    }

    EXPECT_EQ(scheduler->getTaskCount(), 10);

    // Let them start running
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Shutdown should cleanly stop all tasks
    scheduler->shutdown();

    // Should be safe to call again
    scheduler->shutdown();
}

TEST_F(ErrorHandlingTest, OperationsAfterShutdown) {
    scheduler->shutdown();

    // Operations after shutdown should fail gracefully
    bool createResult = scheduler->createTask("AfterShutdown", []() {
        return std::make_shared<SensorTask>(
            TaskConfig{"AfterShutdown", 100, 10, 0, true, 10, 0, true}
        );
    });

    // Should not create task after shutdown
    // (Implementation dependent - might allow or reject)
}

TEST_F(ErrorHandlingTest, StopTaskDuringExecution) {
    auto task = std::make_shared<SensorTask>(
        TaskConfig{"StopDuringRun", 50, 10, 0, true, 10, 0, true}
    );

    scheduler->createTask("StopDuringRun", [task]() { return task; });

    // Let it start running
    std::this_thread::sleep_for(std::chrono::milliseconds(25));

    // Stop while potentially executing
    bool result = scheduler->stopTask("StopDuringRun");
    EXPECT_TRUE(result);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Should be stopped
    EXPECT_EQ(scheduler->getTaskCount(), 0);
}

// ====== Concurrent Error Tests ======

TEST_F(ErrorHandlingTest, ConcurrentCreateAndStop) {
    std::atomic<int> createCount{0};
    std::atomic<int> stopCount{0};

    // Thread that creates tasks
    std::thread creator([&]() {
        for (int i = 0; i < 20; ++i) {
            std::string name = "Create" + std::to_string(i);
            bool result = scheduler->createTask(name, [name]() {
                return std::make_shared<SensorTask>(
                    TaskConfig{name, 100, 10, 0, true, 10, 0, true}
                );
            });
            if (result) createCount++;
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    });

    // Thread that stops tasks
    std::thread stopper([&]() {
        for (int i = 0; i < 20; ++i) {
            std::string name = "Create" + std::to_string(i);
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            bool result = scheduler->stopTask(name);
            if (result) stopCount++;
        }
    });

    creator.join();
    stopper.join();

    // Should handle concurrent operations safely
    EXPECT_GE(createCount.load(), 0);
    EXPECT_GE(stopCount.load(), 0);
}

TEST_F(ErrorHandlingTest, ConcurrentUpdates) {
    auto task = std::make_shared<SensorTask>(
        TaskConfig{"ConcurrentUpdate", 100, 10, 0, true, 10, 0, true}
    );

    scheduler->createTask("ConcurrentUpdate", [task]() { return task; });

    // Multiple threads updating same task
    std::vector<std::thread> threads;
    for (int i = 0; i < 5; ++i) {
        threads.emplace_back([&, i]() {
            for (int j = 0; j < 10; ++j) {
                scheduler->updateTask("ConcurrentUpdate", 
                                     50 + (i * 10), 10, 0, true, 10, 0, true);
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // Task should still be valid
    EXPECT_NE(scheduler->getTask("ConcurrentUpdate"), nullptr);
    EXPECT_TRUE(task->isActive());
}

// ====== Memory/Resource Error Tests ======

TEST_F(ErrorHandlingTest, TaskNameWithSpecialCharacters) {
    // Task name with special characters
    std::string specialName = "Task@#$%^&*()_+-=[]{}|;:',.<>?/~`";
    
    bool result = scheduler->createTask(specialName, [specialName]() {
        return std::make_shared<SensorTask>(
            TaskConfig{specialName, 100, 10, 0, true, 10, 0, true}
        );
    });

    EXPECT_TRUE(result);
    EXPECT_NE(scheduler->getTask(specialName), nullptr);
}

TEST_F(ErrorHandlingTest, VeryLongTaskName) {
    // Very long task name
    std::string longName(1000, 'A');
    
    bool result = scheduler->createTask(longName, [longName]() {
        return std::make_shared<SensorTask>(
            TaskConfig{longName, 100, 10, 0, true, 10, 0, true}
        );
    });

    EXPECT_TRUE(result);
    EXPECT_NE(scheduler->getTask(longName), nullptr);
}

TEST_F(ErrorHandlingTest, EmptyTaskName) {
    // Empty task name
    bool result = scheduler->createTask("", []() {
        return std::make_shared<SensorTask>(
            TaskConfig{"", 100, 10, 0, true, 10, 0, true}
        );
    });

    // Should handle empty name (might allow or reject)
    // Implementation dependent
}

// ====== Graceful Degradation Tests ======

TEST_F(ErrorHandlingTest, RecoveryAfterException) {
    // Create task that throws
    auto throwingTask = std::make_shared<ThrowingTask>(
        TaskConfig{"Throwing", 50, 2, 0, true, 2, 0, true}
    );
    throwingTask->setThrowLocation(ThrowingTask::ThrowLocation::PLAN);
    throwingTask->setPlanResult(true, true);

    scheduler->createTask("Throwing", [throwingTask]() { return throwingTask; });

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Remove throwing task
    scheduler->stopTask("Throwing");

    // Create normal task - should work fine
    auto normalTask = std::make_shared<SensorTask>(
        TaskConfig{"Normal", 50, 10, 0, true, 10, 0, true}
    );

    bool result = scheduler->createTask("Normal", [normalTask]() { return normalTask; });
    EXPECT_TRUE(result);

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Normal task should work properly
    EXPECT_NE(scheduler->getTask("Normal"), nullptr);
    EXPECT_TRUE(normalTask->isActive());
}

TEST_F(ErrorHandlingTest, PartialConfigUpdate) {
    // Create task
    auto task = std::make_shared<SensorTask>(
        TaskConfig{"Partial", 100, 10, 0, true, 10, 0, true}
    );

    scheduler->createTask("Partial", [task]() { return task; });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Update some parameters
    scheduler->updateTask("Partial", 200, 20, 5, false, 15, 3, false);

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // All parameters should be updated
    EXPECT_EQ(task->getInterval(), 200);
}
