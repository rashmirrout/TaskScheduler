#include "sensor_task.h"
#include <iostream>
#include <iomanip>
#include <chrono>

namespace task_scheduler {

SensorTask::SensorTask(const std::string& name, 
                       int intervalMs,
                       double threshold)
    : TaskBase(name, intervalMs)
    , sensorValue_(0.0)
    , threshold_(threshold)
{
}

PlanResult SensorTask::plan() {
    // Simulate sensor reading logic
    // Return true for signal if sensor value exceeds threshold
    double value = sensorValue_.load();
    bool shouldSignal = (value > threshold_);
    
    // For this demo, we'll use signal channel primarily
    // Action channel can be used for additional processing
    return {shouldSignal, shouldSignal};
}

void SensorTask::signal(bool doSignal) {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::tm tm_buf;
    #ifdef _WIN32
        localtime_s(&tm_buf, &time_t);
    #else
        localtime_r(&time_t, &tm_buf);
    #endif
    
    if (doSignal) {
        std::cout << "[" 
                  << std::put_time(&tm_buf, "%H:%M:%S")
                  << "." << std::setfill('0') << std::setw(3) << ms.count()
                  << "] [" << getName() << "] "
                  << "SIGNAL ACTIVATED (value=" << sensorValue_.load() 
                  << ", threshold=" << threshold_ << ")\n";
    } else {
        std::cout << "[" 
                  << std::put_time(&tm_buf, "%H:%M:%S")
                  << "." << std::setfill('0') << std::setw(3) << ms.count()
                  << "] [" << getName() << "] "
                  << "SIGNAL DEACTIVATED (value=" << sensorValue_.load() << ")\n";
    }
}

void SensorTask::act(bool doAct) {
    if (doAct) {
        std::cout << "[" << getName() << "] Processing sensor data...\n";
    } else {
        std::cout << "[" << getName() << "] Stopped processing\n";
    }
}

} // namespace task_scheduler
