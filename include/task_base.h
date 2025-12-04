#pragma once

#include "types.h"
#include <string>
#include <atomic>
#include <mutex>

namespace task_scheduler {

/**
 * @brief Abstract base class for all tasks
 * 
 * Implements the Template Method pattern with sophisticated state machine logic.
 * The run() method handles all timing, debouncing, heartbeat, and safety gate logic.
 * Derived classes only implement business logic (plan, signal, act).
 */
class TaskBase {
public:
    /**
     * @brief Constructor
     * @param config Task configuration containing all parameters including task name
     */
    explicit TaskBase(const TaskConfig& config);
    
    virtual ~TaskBase() = default;

    // ====== Template Method Pattern ======
    /**
     * @brief Main execution method called by scheduler
     * Implements the complete state machine with:
     * - Configuration snapshotting (thread-safe)
     * - Signal channel processing (debounce, heartbeat, gates)
     * - Action channel processing (independent state machine)
     */
    void run();

    // ====== Abstract Interface (Business Logic) ======
    /**
     * @brief Derived class reports its intent
     * @return PlanResult containing wantSignal and wantAct flags
     */
    virtual PlanResult plan() = 0;

    /**
     * @brief Signal channel state change
     * @param doSignal True = activate, False = deactivate
     */
    virtual void signal(bool doSignal) = 0;

    /**
     * @brief Action channel state change
     * @param doAct True = activate, False = deactivate
     */
    virtual void act(bool doAct) = 0;

    // ====== Configuration Management (Thread-Safe) ======
    /**
     * @brief Update task configuration (individual parameters)
     * Thread-safe via mutex
     */
    void updateConfig(int intervalMs,
                     int sigTolerance,
                     int sigRepeat,
                     bool allowSignal,
                     int actTolerance,
                     int actRepeat,
                     bool allowAction);

    /**
     * @brief Update task configuration (using TaskConfig struct)
     * Thread-safe via mutex
     */
    void updateConfig(const TaskConfig& config);

    /**
     * @brief Get current interval (thread-safe)
     * Used by scheduler for rescheduling
     */
    int getInterval() const;

    /**
     * @brief Get task name
     */
    const std::string& getName() const { return name_; }

    /**
     * @brief Check if task is active
     */
    bool isActive() const { return active_.load(); }

    /**
     * @brief Set task active state
     */
    void setActive(bool active) { active_.store(active); }

private:
    // ====== State Machine Implementation ======
    void processSignalChannel(const TaskConfig& cfg, bool wantSignal);
    void processActionChannel(const TaskConfig& cfg, bool wantAct);

    // ====== Task Identity ======
    std::string name_;
    std::atomic<bool> active_;

    // ====== Configuration (Mutex Protected) ======
    mutable std::mutex configMutex_;
    TaskConfig config_;  // Single struct holding all configuration

    // ====== State Machine State (No Mutex - Only Accessed by run()) ======
    // Signal channel state
    int sigCounter_;
    bool isSignaled_;
    
    // Action channel state
    int actCounter_;
    bool isActing_;
};

} // namespace task_scheduler
