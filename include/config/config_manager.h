#pragma once

#include "core/scheduler.h"
#include "config/config_parser.h"
#include "config/file_watcher.h"
#include "tasks/task_factory.h"
#include <string>
#include <vector>
#include <mutex>
#include <chrono>
#include <thread>
#include <atomic>
#include <unordered_map>

namespace task_scheduler {

/**
 * @brief Manages configuration-driven task lifecycle with file watching
 * Handles loading, updating, and synchronizing tasks from XML configuration
 */
class ConfigManager {
public:
    /**
     * @brief Construct configuration manager
     * @param scheduler Reference to scheduler to manage
     * @param configPath Path to XML configuration file
     * @param debounceWindow Time window for debouncing rapid changes (default: 5 minutes)
     */
    ConfigManager(Scheduler& scheduler, 
                  const std::string& configPath,
                  std::chrono::minutes debounceWindow = std::chrono::minutes(5));
    
    /**
     * @brief Destructor - stops watching and cleanup
     */
    ~ConfigManager();
    
    /**
     * @brief Start configuration management
     * Loads initial config and starts file watching
     * @return true if initial load successful, false otherwise
     */
    bool start();
    
    /**
     * @brief Stop configuration management
     * Stops file watching and debounce thread
     */
    void stop();
    
    /**
     * @brief Get current number of managed tasks
     * @return Number of tasks
     */
    size_t getTaskCount() const;
    
private:
    /**
     * @brief Callback when file changes detected
     */
    void onFileChanged();
    
    /**
     * @brief Apply pending configuration changes (after debounce)
     */
    void applyPendingChanges();
    
    /**
     * @brief Synchronize tasks based on new configurations
     * @param newConfigs New configuration to apply
     */
    void syncTasks(const std::vector<ExtendedTaskConfig>& newConfigs);
    
    /**
     * @brief Debounce thread main loop
     */
    void debounceLoop();
    
    Scheduler& scheduler_;
    std::string configPath_;
    std::chrono::minutes debounceWindow_;
    
    std::unique_ptr<FileWatcher> watcher_;
    std::vector<ExtendedTaskConfig> currentConfigs_;
    
    // Debouncing state
    std::atomic<bool> pendingUpdate_;
    std::chrono::steady_clock::time_point lastChangeTime_;
    std::atomic<bool> running_;
    std::thread debounceThread_;
    
    // Thread safety
    mutable std::mutex configMutex_;
};

} // namespace task_scheduler
