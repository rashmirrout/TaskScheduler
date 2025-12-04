#pragma once

#include <string>
#include <memory>
#include <chrono>

namespace task_scheduler {

// Forward declaration
class TaskBase;

/**
 * @brief Result of plan() method indicating task intent
 */
struct PlanResult {
    bool wantSignal;  // True if signal channel should be active
    bool wantAct;     // True if action channel should be active
};

/**
 * @brief Thread-safe snapshot of task configuration
 * Used to minimize critical section in run() method
 */
struct TaskConfig {
    int intervalMs;
    
    // Signal channel configuration
    int sigTolerance;     // Number of consecutive "true" plans needed to activate
    int sigRepeat;        // Heartbeat interval (0 = single shot, no repeat)
    bool allowSignal;     // Global safety gate for signal channel
    
    // Action channel configuration
    int actTolerance;     // Number of consecutive "true" plans needed to activate
    int actRepeat;        // Heartbeat interval (0 = single shot, no repeat)
    bool allowAction;     // Global safety gate for action channel
};

/**
 * @brief Entry in the priority queue for scheduled tasks
 */
struct ScheduleEntry {
    std::chrono::steady_clock::time_point nextRunTime;
    std::shared_ptr<TaskBase> task;
    
    // Priority queue needs greater-than for min-heap (earliest time = highest priority)
    bool operator>(const ScheduleEntry& other) const {
        return nextRunTime > other.nextRunTime;
    }
};

} // namespace task_scheduler
