#pragma once

#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <chrono>
#include <filesystem>

namespace task_scheduler {

/**
 * @brief Cross-platform file watcher using polling
 * Monitors a file for modifications and invokes callback on changes
 */
class FileWatcher {
public:
    /**
     * @brief Construct file watcher
     * @param filePath Path to file to watch
     * @param callback Function to call when file changes
     * @param pollInterval How often to check for changes (default: 1 second)
     */
    FileWatcher(const std::string& filePath, 
                std::function<void()> callback,
                std::chrono::seconds pollInterval = std::chrono::seconds(1));
    
    /**
     * @brief Destructor - stops watching if still running
     */
    ~FileWatcher();
    
    /**
     * @brief Start watching the file
     */
    void start();
    
    /**
     * @brief Stop watching the file
     */
    void stop();
    
    /**
     * @brief Check if watcher is currently running
     * @return true if watching, false otherwise
     */
    bool isRunning() const;
    
private:
    /**
     * @brief Main watch loop (runs in separate thread)
     */
    void watchLoop();
    
    /**
     * @brief Get current file modification time
     * @return File modification time, or min time if file doesn't exist
     */
    std::filesystem::file_time_type getFileModTime() const;
    
    std::string filePath_;
    std::function<void()> callback_;
    std::chrono::seconds pollInterval_;
    
    std::atomic<bool> running_;
    std::thread watchThread_;
    std::filesystem::file_time_type lastModTime_;
};

} // namespace task_scheduler
