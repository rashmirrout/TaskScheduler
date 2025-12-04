#include "config/file_watcher.h"
#include <iostream>

namespace task_scheduler {

FileWatcher::FileWatcher(const std::string& filePath, 
                         std::function<void()> callback,
                         std::chrono::seconds pollInterval)
    : filePath_(filePath)
    , callback_(callback)
    , pollInterval_(pollInterval)
    , running_(false)
    , lastModTime_(std::filesystem::file_time_type::min())
{
}

FileWatcher::~FileWatcher() {
    stop();
}

void FileWatcher::start() {
    if (running_.load()) {
        std::cerr << "FileWatcher: Already running" << std::endl;
        return;
    }
    
    // Initialize last modification time
    lastModTime_ = getFileModTime();
    
    running_ = true;
    watchThread_ = std::thread(&FileWatcher::watchLoop, this);
    
    std::cout << "FileWatcher: Started watching " << filePath_ << std::endl;
}

void FileWatcher::stop() {
    if (!running_.load()) {
        return;
    }
    
    running_ = false;
    
    if (watchThread_.joinable()) {
        watchThread_.join();
    }
    
    std::cout << "FileWatcher: Stopped watching " << filePath_ << std::endl;
}

bool FileWatcher::isRunning() const {
    return running_.load();
}

void FileWatcher::watchLoop() {
    while (running_.load()) {
        auto currentModTime = getFileModTime();
        
        // Check if file was modified
        if (currentModTime != lastModTime_ && 
            currentModTime != std::filesystem::file_time_type::min()) {
            
            std::cout << "FileWatcher: File change detected: " << filePath_ << std::endl;
            lastModTime_ = currentModTime;
            
            // Invoke callback
            if (callback_) {
                callback_();
            }
        }
        
        // Sleep for poll interval
        std::this_thread::sleep_for(pollInterval_);
    }
}

std::filesystem::file_time_type FileWatcher::getFileModTime() const {
    try {
        if (std::filesystem::exists(filePath_)) {
            return std::filesystem::last_write_time(filePath_);
        }
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "FileWatcher: Error getting file modification time: " 
                  << e.what() << std::endl;
    }
    
    return std::filesystem::file_time_type::min();
}

} // namespace task_scheduler
