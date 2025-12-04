#include <gtest/gtest.h>
#include "config/config_parser.h"
#include "config/file_watcher.h"
#include "config/config_manager.h"
#include "core/scheduler.h"
#include <fstream>
#include <filesystem>
#include <thread>
#include <chrono>

using namespace task_scheduler;
namespace fs = std::filesystem;

// ====== Test Fixtures ======

class ConfigParserTest : public ::testing::Test {
protected:
    void SetUp() override {
        testDir_ = fs::temp_directory_path() / "taskscheduler_test";
        fs::create_directories(testDir_);
    }

    void TearDown() override {
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

    fs::path testDir_;
};

class FileWatcherTest : public ::testing::Test {
protected:
    void SetUp() override {
        testDir_ = fs::temp_directory_path() / "taskscheduler_watcher";
        fs::create_directories(testDir_);
        testFile_ = testDir_ / "watch_test.txt";
    }

    void TearDown() override {
        if (fs::exists(testDir_)) {
            fs::remove_all(testDir_);
        }
    }

    void createFile(const std::string& content = "initial") {
        std::ofstream file(testFile_);
        file << content;
        file.close();
    }

    void modifyFile(const std::string& content) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1100)); // Ensure time difference
        std::ofstream file(testFile_);
        file << content;
        file.close();
    }

    fs::path testDir_;
    fs::path testFile_;
};

class ConfigManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        testDir_ = fs::temp_directory_path() / "taskscheduler_manager";
        fs::create_directories(testDir_);
        configFile_ = testDir_ / "config.xml";
        scheduler_ = std::make_unique<Scheduler>(2);
    }

    void TearDown() override {
        manager_.reset();
        scheduler_.reset();
        if (fs::exists(testDir_)) {
            fs::remove_all(testDir_);
        }
    }

    void writeConfig(const std::string& content) {
        std::ofstream file(configFile_);
        file << content;
        file.close();
    }

    fs::path testDir_;
    fs::path configFile_;
    std::unique_ptr<Scheduler> scheduler_;
    std::unique_ptr<ConfigManager> manager_;
};

// ====== ConfigParser Tests ======

TEST_F(ConfigParserTest, ValidXMLParsing) {
    std::string xml = R"(<?xml version="1.0"?>
<tasks>
    <task name="Sensor1" type="SensorTask" intervalMs="100" 
          sigTolerance="10" sigRepeat="5" allowSignal="true"
          actTolerance="8" actRepeat="3" allowAction="false"/>
    <task name="Actuator1" type="ActuatorTask" intervalMs="200"
          sigTolerance="15" sigRepeat="0" allowSignal="false"
          actTolerance="12" actRepeat="0" allowAction="true"/>
</tasks>)";

    writeXMLFile("valid.xml", xml);
    auto configs = ConfigParser::parse(getTestPath("valid.xml"));

    ASSERT_EQ(configs.size(), 2);
    
    // Verify first task
    EXPECT_EQ(configs[0].config.taskName, "Sensor1");
    EXPECT_EQ(configs[0].taskType, "SensorTask");
    EXPECT_EQ(configs[0].config.intervalMs, 100);
    EXPECT_EQ(configs[0].config.sigTolerance, 10);
    EXPECT_EQ(configs[0].config.sigRepeat, 5);
    EXPECT_TRUE(configs[0].config.allowSignal);
    EXPECT_EQ(configs[0].config.actTolerance, 8);
    EXPECT_EQ(configs[0].config.actRepeat, 3);
    EXPECT_FALSE(configs[0].config.allowAction);

    // Verify second task
    EXPECT_EQ(configs[1].config.taskName, "Actuator1");
    EXPECT_EQ(configs[1].taskType, "ActuatorTask");
    EXPECT_EQ(configs[1].config.intervalMs, 200);
}

TEST_F(ConfigParserTest, MissingOptionalParameters) {
    std::string xml = R"(<?xml version="1.0"?>
<tasks>
    <task name="MinimalTask" type="SensorTask" intervalMs="100"/>
</tasks>)";

    writeXMLFile("minimal.xml", xml);
    auto configs = ConfigParser::parse(getTestPath("minimal.xml"));

    ASSERT_EQ(configs.size(), 1);
    
    // Verify defaults applied
    EXPECT_EQ(configs[0].config.taskName, "MinimalTask");
    EXPECT_EQ(configs[0].config.intervalMs, 100);
    EXPECT_EQ(configs[0].config.sigTolerance, 10);  // Default
    EXPECT_EQ(configs[0].config.sigRepeat, 0);      // Default
    EXPECT_TRUE(configs[0].config.allowSignal);     // Default
    EXPECT_EQ(configs[0].config.actTolerance, 10);  // Default
    EXPECT_EQ(configs[0].config.actRepeat, 0);      // Default
    EXPECT_TRUE(configs[0].config.allowAction);     // Default
}

TEST_F(ConfigParserTest, InvalidTaskType) {
    std::string xml = R"(<?xml version="1.0"?>
<tasks>
    <task name="Invalid" type="UnknownTask" intervalMs="100"/>
</tasks>)";

    writeXMLFile("invalid_type.xml", xml);
    auto configs = ConfigParser::parse(getTestPath("invalid_type.xml"));

    // Invalid task type should be rejected
    EXPECT_TRUE(configs.empty());
}

TEST_F(ConfigParserTest, NegativeIntervalMs) {
    std::string xml = R"(<?xml version="1.0"?>
<tasks>
    <task name="NegInterval" type="SensorTask" intervalMs="-100"/>
</tasks>)";

    writeXMLFile("negative_interval.xml", xml);
    auto configs = ConfigParser::parse(getTestPath("negative_interval.xml"));

    // Negative interval should be rejected
    EXPECT_TRUE(configs.empty());
}

TEST_F(ConfigParserTest, MalformedXML) {
    std::string xml = R"(<?xml version="1.0"?>
<tasks>
    <task name="Broken" type="SensorTask" intervalMs="100"
</tasks>)";  // Missing closing tag

    writeXMLFile("malformed.xml", xml);
    auto configs = ConfigParser::parse(getTestPath("malformed.xml"));

    // Malformed XML should return empty
    EXPECT_TRUE(configs.empty());
}

TEST_F(ConfigParserTest, EmptyConfigFile) {
    std::string xml = R"(<?xml version="1.0"?>
<tasks>
</tasks>)";

    writeXMLFile("empty.xml", xml);
    auto configs = ConfigParser::parse(getTestPath("empty.xml"));

    // Empty config should return empty vector (not an error)
    EXPECT_TRUE(configs.empty());
}

TEST_F(ConfigParserTest, DuplicateTaskNames) {
    std::string xml = R"(<?xml version="1.0"?>
<tasks>
    <task name="Duplicate" type="SensorTask" intervalMs="100"/>
    <task name="Duplicate" type="ActuatorTask" intervalMs="200"/>
</tasks>)";

    writeXMLFile("duplicates.xml", xml);
    auto configs = ConfigParser::parse(getTestPath("duplicates.xml"));

    // Parser should accept both (Scheduler will handle duplicates)
    EXPECT_EQ(configs.size(), 2);
}

TEST_F(ConfigParserTest, ExtremeToleranceValues) {
    std::string xml = R"(<?xml version="1.0"?>
<tasks>
    <task name="Zero" type="SensorTask" intervalMs="100" sigTolerance="0"/>
    <task name="One" type="SensorTask" intervalMs="100" sigTolerance="1"/>
    <task name="Large" type="SensorTask" intervalMs="100" sigTolerance="1000"/>
</tasks>)";

    writeXMLFile("extreme_tolerance.xml", xml);
    auto configs = ConfigParser::parse(getTestPath("extreme_tolerance.xml"));

    ASSERT_EQ(configs.size(), 3);
    EXPECT_EQ(configs[0].config.sigTolerance, 0);
    EXPECT_EQ(configs[1].config.sigTolerance, 1);
    EXPECT_EQ(configs[2].config.sigTolerance, 1000);
}

TEST_F(ConfigParserTest, BooleanParsing) {
    std::string xml = R"(<?xml version="1.0"?>
<tasks>
    <task name="T1" type="SensorTask" intervalMs="100" allowSignal="true" allowAction="false"/>
    <task name="T2" type="SensorTask" intervalMs="100" allowSignal="1" allowAction="0"/>
    <task name="T3" type="SensorTask" intervalMs="100" allowSignal="yes" allowAction="no"/>
</tasks>)";

    writeXMLFile("booleans.xml", xml);
    auto configs = ConfigParser::parse(getTestPath("booleans.xml"));

    ASSERT_EQ(configs.size(), 3);
    
    // "true"/"false"
    EXPECT_TRUE(configs[0].config.allowSignal);
    EXPECT_FALSE(configs[0].config.allowAction);
    
    // "1"/"0"
    EXPECT_TRUE(configs[1].config.allowSignal);
    EXPECT_FALSE(configs[1].config.allowAction);
    
    // "yes"/"no"
    EXPECT_TRUE(configs[2].config.allowSignal);
    EXPECT_FALSE(configs[2].config.allowAction);
}

TEST_F(ConfigParserTest, NonExistentFile) {
    auto configs = ConfigParser::parse(getTestPath("does_not_exist.xml"));
    EXPECT_TRUE(configs.empty());
}

// ====== FileWatcher Tests ======

TEST_F(FileWatcherTest, DetectFileModification) {
    createFile("initial content");
    
    std::atomic<int> callbackCount{0};
    FileWatcher watcher(testFile_.string(), [&callbackCount]() {
        callbackCount++;
    });

    watcher.start();
    EXPECT_TRUE(watcher.isRunning());

    // Modify file
    modifyFile("modified content");

    // Wait for detection
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));

    watcher.stop();
    EXPECT_FALSE(watcher.isRunning());

    // Should have detected at least one change
    EXPECT_GE(callbackCount.load(), 1);
}

TEST_F(FileWatcherTest, MultipleRapidChanges) {
    createFile("initial");
    
    std::atomic<int> callbackCount{0};
    FileWatcher watcher(testFile_.string(), [&callbackCount]() {
        callbackCount++;
    });

    watcher.start();

    // Make multiple changes
    for (int i = 0; i < 3; ++i) {
        modifyFile("change " + std::to_string(i));
    }

    // Wait for all detections
    std::this_thread::sleep_for(std::chrono::milliseconds(4000));

    watcher.stop();

    // Should detect all changes
    EXPECT_GE(callbackCount.load(), 3);
}

TEST_F(FileWatcherTest, FileDeleted) {
    createFile("test");
    
    std::atomic<int> callbackCount{0};
    FileWatcher watcher(testFile_.string(), [&callbackCount]() {
        callbackCount++;
    });

    watcher.start();
    
    // Delete file
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    fs::remove(testFile_);

    // Wait a bit
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));

    // Should handle gracefully (no crash)
    watcher.stop();
    EXPECT_TRUE(true); // If we reach here, no crash occurred
}

TEST_F(FileWatcherTest, FileRecreated) {
    createFile("first");
    
    std::atomic<int> callbackCount{0};
    FileWatcher watcher(testFile_.string(), [&callbackCount]() {
        callbackCount++;
    });

    watcher.start();

    // Delete file
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    fs::remove(testFile_);
    
    // Recreate file
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    createFile("recreated");

    // Wait for detection
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));

    watcher.stop();

    // Should detect recreation
    EXPECT_GE(callbackCount.load(), 1);
}

TEST_F(FileWatcherTest, StopWhileWatching) {
    createFile("test");
    
    FileWatcher watcher(testFile_.string(), []() {});
    
    watcher.start();
    EXPECT_TRUE(watcher.isRunning());
    
    // Immediate stop
    watcher.stop();
    EXPECT_FALSE(watcher.isRunning());
}

TEST_F(FileWatcherTest, DoubleStart) {
    createFile("test");
    
    FileWatcher watcher(testFile_.string(), []() {});
    
    watcher.start();
    EXPECT_TRUE(watcher.isRunning());
    
    // Second start should be safe (no-op or restart)
    watcher.start();
    EXPECT_TRUE(watcher.isRunning());
    
    watcher.stop();
}

TEST_F(FileWatcherTest, NonExistentFileAtStart) {
    // File doesn't exist yet
    FileWatcher watcher(testFile_.string(), []() {});
    
    // Should handle gracefully
    watcher.start();
    EXPECT_TRUE(watcher.isRunning());
    
    watcher.stop();
}

// ====== ConfigManager Tests ======

TEST_F(ConfigManagerTest, InitialLoad) {
    std::string xml = R"(<?xml version="1.0"?>
<tasks>
    <task name="Task1" type="SensorTask" intervalMs="100"/>
    <task name="Task2" type="ActuatorTask" intervalMs="200"/>
</tasks>)";

    writeConfig(xml);
    
    manager_ = std::make_unique<ConfigManager>(*scheduler_, configFile_.string());
    EXPECT_TRUE(manager_->start());

    // Give time for tasks to be created
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_EQ(scheduler_->getTaskCount(), 2);
    EXPECT_NE(scheduler_->getTask("Task1"), nullptr);
    EXPECT_NE(scheduler_->getTask("Task2"), nullptr);
}

TEST_F(ConfigManagerTest, AddTask) {
    std::string initialXml = R"(<?xml version="1.0"?>
<tasks>
    <task name="Task1" type="SensorTask" intervalMs="100"/>
</tasks>)";

    writeConfig(initialXml);
    
    manager_ = std::make_unique<ConfigManager>(*scheduler_, configFile_.string(), 
                                                std::chrono::minutes(0)); // No debounce
    manager_->start();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    EXPECT_EQ(scheduler_->getTaskCount(), 1);

    // Add task
    std::string updatedXml = R"(<?xml version="1.0"?>
<tasks>
    <task name="Task1" type="SensorTask" intervalMs="100"/>
    <task name="Task2" type="ActuatorTask" intervalMs="200"/>
</tasks>)";

    writeConfig(updatedXml);
    
    // Wait for detection and update
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));

    EXPECT_EQ(scheduler_->getTaskCount(), 2);
    EXPECT_NE(scheduler_->getTask("Task2"), nullptr);
}

TEST_F(ConfigManagerTest, RemoveTask) {
    std::string initialXml = R"(<?xml version="1.0"?>
<tasks>
    <task name="Task1" type="SensorTask" intervalMs="100"/>
    <task name="Task2" type="ActuatorTask" intervalMs="200"/>
</tasks>)";

    writeConfig(initialXml);
    
    manager_ = std::make_unique<ConfigManager>(*scheduler_, configFile_.string(),
                                                std::chrono::minutes(0));
    manager_->start();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    EXPECT_EQ(scheduler_->getTaskCount(), 2);

    // Remove task
    std::string updatedXml = R"(<?xml version="1.0"?>
<tasks>
    <task name="Task1" type="SensorTask" intervalMs="100"/>
</tasks>)";

    writeConfig(updatedXml);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));

    EXPECT_EQ(scheduler_->getTaskCount(), 1);
    EXPECT_EQ(scheduler_->getTask("Task2"), nullptr);
}

TEST_F(ConfigManagerTest, UpdateTask) {
    std::string initialXml = R"(<?xml version="1.0"?>
<tasks>
    <task name="Task1" type="SensorTask" intervalMs="100"/>
</tasks>)";

    writeConfig(initialXml);
    
    manager_ = std::make_unique<ConfigManager>(*scheduler_, configFile_.string(),
                                                std::chrono::minutes(0));
    manager_->start();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    auto task1 = scheduler_->getTask("Task1");
    ASSERT_NE(task1, nullptr);
    EXPECT_EQ(task1->getInterval(), 100);

    // Update interval
    std::string updatedXml = R"(<?xml version="1.0"?>
<tasks>
    <task name="Task1" type="SensorTask" intervalMs="500"/>
</tasks>)";

    writeConfig(updatedXml);
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));

    EXPECT_EQ(task1->getInterval(), 500);
}

TEST_F(ConfigManagerTest, MixedOperations) {
    std::string initialXml = R"(<?xml version="1.0"?>
<tasks>
    <task name="Task1" type="SensorTask" intervalMs="100"/>
    <task name="Task2" type="ActuatorTask" intervalMs="200"/>
</tasks>)";

    writeConfig(initialXml);
    
    manager_ = std::make_unique<ConfigManager>(*scheduler_, configFile_.string(),
                                                std::chrono::minutes(0));
    manager_->start();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    EXPECT_EQ(scheduler_->getTaskCount(), 2);

    // Add, update, remove in one change
    std::string updatedXml = R"(<?xml version="1.0"?>
<tasks>
    <task name="Task1" type="SensorTask" intervalMs="300"/>
    <task name="Task3" type="SensorTask" intervalMs="150"/>
</tasks>)";

    writeConfig(updatedXml);
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));

    // Task1 updated, Task2 removed, Task3 added
    EXPECT_EQ(scheduler_->getTaskCount(), 2);
    
    auto task1 = scheduler_->getTask("Task1");
    ASSERT_NE(task1, nullptr);
    EXPECT_EQ(task1->getInterval(), 300);
    
    EXPECT_EQ(scheduler_->getTask("Task2"), nullptr);
    EXPECT_NE(scheduler_->getTask("Task3"), nullptr);
}

TEST_F(ConfigManagerTest, InvalidUpdateRollback) {
    std::string validXml = R"(<?xml version="1.0"?>
<tasks>
    <task name="Task1" type="SensorTask" intervalMs="100"/>
</tasks>)";

    writeConfig(validXml);
    
    manager_ = std::make_unique<ConfigManager>(*scheduler_, configFile_.string(),
                                                std::chrono::minutes(0));
    manager_->start();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    EXPECT_EQ(scheduler_->getTaskCount(), 1);

    // Write invalid XML
    std::string invalidXml = R"(<?xml version="1.0"?>
<tasks>
    <task name="Task1" type="SensorTask" intervalMs="-100"/>
</tasks>)";

    writeConfig(invalidXml);
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));

    // Should keep old configuration
    EXPECT_EQ(scheduler_->getTaskCount(), 1);
    auto task1 = scheduler_->getTask("Task1");
    ASSERT_NE(task1, nullptr);
    EXPECT_EQ(task1->getInterval(), 100); // Original value
}

TEST_F(ConfigManagerTest, StopDuringDebounce) {
    std::string xml = R"(<?xml version="1.0"?>
<tasks>
    <task name="Task1" type="SensorTask" intervalMs="100"/>
</tasks>)";

    writeConfig(xml);
    
    manager_ = std::make_unique<ConfigManager>(*scheduler_, configFile_.string(),
                                                std::chrono::minutes(5)); // Long debounce
    manager_->start();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Modify file
    writeConfig(xml); // Same content, but triggers change
    
    // Stop immediately during debounce window
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    manager_->stop();

    // Should stop gracefully
    EXPECT_TRUE(true);
}

TEST_F(ConfigManagerTest, EmptyInitialConfig) {
    std::string xml = R"(<?xml version="1.0"?>
<tasks>
</tasks>)";

    writeConfig(xml);
    
    manager_ = std::make_unique<ConfigManager>(*scheduler_, configFile_.string());
    EXPECT_TRUE(manager_->start());

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_EQ(scheduler_->getTaskCount(), 0);
}
