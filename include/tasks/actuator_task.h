#pragma once

#include "core/task_base.h"
#include <atomic>

namespace task_scheduler {

/**
 * @brief Concrete task simulating an actuator control
 * 
 * Demonstrates action channel usage with command-based control
 */
class ActuatorTask : public TaskBase {
public:
    /**
     * @brief Constructor
     * @param config Task configuration
     */
    explicit ActuatorTask(const TaskConfig& config);

    // Override abstract methods
    PlanResult plan() override;
    void signal(bool doSignal) override;
    void act(bool doAct) override;

    // ====== Test/Demo Interface ======
    /**
     * @brief Set command state (for testing/demo)
     */
    void setCommand(bool enabled) { commandEnabled_.store(enabled); }

    /**
     * @brief Get current command state
     */
    bool getCommand() const { return commandEnabled_.load(); }

    /**
     * @brief Get action count (for verification)
     */
    int getActionCount() const { return actionCount_.load(); }

private:
    std::atomic<bool> commandEnabled_;  // Command to actuator
    std::atomic<int> actionCount_;      // Count of actions performed
};

} // namespace task_scheduler
