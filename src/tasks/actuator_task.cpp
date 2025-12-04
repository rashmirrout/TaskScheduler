#include "tasks/actuator_task.h"
#include "core/task_base.h"
#include <iostream>
#include <iomanip>
#include <chrono>

namespace task_scheduler {

ActuatorTask::ActuatorTask(const TaskConfig& config)
    : TaskBase(config)
    , commandEnabled_(false)
    , actionCount_(0)
{
}

PlanResult ActuatorTask::plan() {
    // Simulate actuator control logic
    // Return true for action if command is enabled
    bool shouldAct = commandEnabled_.load();
    
    // For this demo, we'll use action channel primarily
    // Signal channel can be used for state reporting
    return {shouldAct, shouldAct};
}

void ActuatorTask::signal(bool doSignal) {
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
                  << "State: READY\n";
    } else {
        std::cout << "[" 
                  << std::put_time(&tm_buf, "%H:%M:%S")
                  << "." << std::setfill('0') << std::setw(3) << ms.count()
                  << "] [" << getName() << "] "
                  << "State: IDLE\n";
    }
}

void ActuatorTask::act(bool doAct) {
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
    
    if (doAct) {
        int count = actionCount_.fetch_add(1) + 1;
        std::cout << "[" 
                  << std::put_time(&tm_buf, "%H:%M:%S")
                  << "." << std::setfill('0') << std::setw(3) << ms.count()
                  << "] [" << getName() << "] "
                  << "ACTION EXECUTED (count=" << count << ")\n";
    } else {
        std::cout << "[" 
                  << std::put_time(&tm_buf, "%H:%M:%S")
                  << "." << std::setfill('0') << std::setw(3) << ms.count()
                  << "] [" << getName() << "] "
                  << "ACTION STOPPED (total=" << actionCount_.load() << ")\n";
    }
}

} // namespace task_scheduler
