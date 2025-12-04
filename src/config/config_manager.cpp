#include "config/config_manager.h"
#include "core/scheduler.h"
#include "config/config_parser.h"
#include "config/file_watcher.h"
#include "tasks/task_factory.h"
#include <iostream>
#include <unordered_set>

namespace task_scheduler {

ConfigManager::ConfigManager(Scheduler& scheduler, 
                             const std::string& configPath,
                             std::chrono::minutes debounceWindow)
    : scheduler_(scheduler)
    , configPath_(configPath)
    , debounceWindow_(debounceWindow)
    , pendingUpdate_(false)
    , running_(false)
{
}

ConfigManager::~ConfigManager() {
    stop();
}

bool ConfigManager::start() {
    std::cout << "ConfigManager: Starting..." << std::endl;
    
    // Load initial configuration
    auto configs = ConfigParser::parse(configPath_);
    if (configs.empty()) {
        std::cerr << "ConfigManager: Failed to load initial configuration from " 
                  << configPath_ << std::endl;
        return false;
    }
    
    // Create initial tasks
    syncTasks(configs);
    
    // Start file watcher
    watcher_ = std::make_unique<FileWatcher>(
        configPath_,
        [this]() { onFileChanged(); },
        std::chrono::seconds(1)
    );
    watcher_->start();
    
    // Start debounce thread
    running_ = true;
    debounceThread_ = std::thread(&ConfigManager::debounceLoop, this);
    
    std::cout << "ConfigManager: Started successfully with " 
              << currentConfigs_.size() << " task(s)" << std::endl;
    
    return true;
}

void ConfigManager::stop() {
    if (!running_.load()) {
        return;
    }
    
    std::cout << "ConfigManager: Stopping..." << std::endl;
    
    // Stop file watcher
    if (watcher_) {
        watcher_->stop();
        watcher_.reset();
    }
    
    // Stop debounce thread
    running_ = false;
    if (debounceThread_.joinable()) {
        debounceThread_.join();
    }
    
    std::cout << "ConfigManager: Stopped" << std::endl;
}

size_t ConfigManager::getTaskCount() const {
    std::lock_guard<std::mutex> lock(configMutex_);
    return currentConfigs_.size();
}

void ConfigManager::onFileChanged() {
    std::lock_guard<std::mutex> lock(configMutex_);
    
    std::cout << "ConfigManager: Configuration file changed, debouncing..." << std::endl;
    
    // Mark that we have a pending update
    pendingUpdate_ = true;
    lastChangeTime_ = std::chrono::steady_clock::now();
}

void ConfigManager::debounceLoop() {
    while (running_.load()) {
        // Check if we have a pending update
        if (pendingUpdate_.load()) {
            std::unique_lock<std::mutex> lock(configMutex_);
            
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::minutes>(
                now - lastChangeTime_
            );
            
            // Apply update if debounce window has passed
            if (elapsed >= debounceWindow_) {
                std::cout << "ConfigManager: Debounce window elapsed, applying changes..." 
                          << std::endl;
                
                pendingUpdate_ = false;
                lock.unlock();  // Unlock before calling applyPendingChanges
                
                applyPendingChanges();
            }
        }
        
        // Sleep for a short interval before checking again
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

void ConfigManager::applyPendingChanges() {
    // Parse new configuration
    auto newConfigs = ConfigParser::parse(configPath_);
    
    if (newConfigs.empty()) {
        std::cerr << "ConfigManager: Failed to parse updated configuration, "
                  << "keeping existing tasks" << std::endl;
        return;
    }
    
    // Synchronize tasks
    syncTasks(newConfigs);
}

void ConfigManager::syncTasks(const std::vector<ExtendedTaskConfig>& newConfigs) {
    std::lock_guard<std::mutex> lock(configMutex_);
    
    std::cout << "\n=== ConfigManager: Synchronizing Tasks ===" << std::endl;
    
    // Build maps for efficient lookup
    std::unordered_map<std::string, ExtendedTaskConfig> oldConfigMap;
    std::unordered_map<std::string, ExtendedTaskConfig> newConfigMap;
    
    for (const auto& config : currentConfigs_) {
        oldConfigMap[config.config.taskName] = config;
    }
    
    for (const auto& config : newConfigs) {
        newConfigMap[config.config.taskName] = config;
    }
    
    // Track changes
    int added = 0, updated = 0, removed = 0;
    
    // 1. Process new tasks (add or update)
    for (const auto& [name, newConfig] : newConfigMap) {
        auto it = oldConfigMap.find(name);
        
        if (it == oldConfigMap.end()) {
            // ADD: Task doesn't exist, create it
            std::cout << "  [ADD] Creating task: " << name << std::endl;
            
            auto task = TaskFactory::create(newConfig);
            if (task) {
                bool created = scheduler_.createTask(name, [task]() {
                    return task;
                });
                
                if (created) {
                    added++;
                } else {
                    std::cerr << "    Failed to create task: " << name << std::endl;
                }
            }
        } 
        else if (it->second != newConfig) {
            // UPDATE: Task exists but config changed
            std::cout << "  [UPDATE] Updating task: " << name << std::endl;
            
            bool updated_success = scheduler_.updateTask(
                name,
                newConfig.config.intervalMs,
                newConfig.config.sigTolerance,
                newConfig.config.sigRepeat,
                newConfig.config.allowSignal,
                newConfig.config.actTolerance,
                newConfig.config.actRepeat,
                newConfig.config.allowAction
            );
            
            if (updated_success) {
                updated++;
            } else {
                std::cerr << "    Failed to update task: " << name << std::endl;
            }
        }
        // else: Task exists and config unchanged, no action needed
    }
    
    // 2. Process removed tasks
    for (const auto& [name, oldConfig] : oldConfigMap) {
        if (newConfigMap.find(name) == newConfigMap.end()) {
            // REMOVE: Task exists in old but not in new
            std::cout << "  [REMOVE] Deleting task: " << name << std::endl;
            
            bool stopped = scheduler_.stopTask(name);
            if (stopped) {
                removed++;
            } else {
                std::cerr << "    Failed to remove task: " << name << std::endl;
            }
        }
    }
    
    // Update current configs
    currentConfigs_ = newConfigs;
    
    std::cout << "=== Synchronization Complete ===" << std::endl;
    std::cout << "  Added: " << added << ", Updated: " << updated 
              << ", Removed: " << removed << std::endl;
    std::cout << "  Total tasks: " << scheduler_.getTaskCount() << "\n" << std::endl;
}

} // namespace task_scheduler
