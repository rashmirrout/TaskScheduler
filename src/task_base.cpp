#include "task_base.h"

namespace task_scheduler {

TaskBase::TaskBase(const std::string& name,
                   int intervalMs,
                   int sigTolerance,
                   int sigRepeat,
                   int actTolerance,
                   int actRepeat)
    : name_(name)
    , active_(true)
    , config_{intervalMs, sigTolerance, sigRepeat, true, actTolerance, actRepeat, true}
    , sigCounter_(0)
    , isSignaled_(false)
    , actCounter_(0)
    , isActing_(false)
{
}

void TaskBase::run() {
    // Early exit if task is inactive
    if (!active_.load()) {
        return;
    }

    // ====== STEP 1: Configuration Snapshotting ======
    // Lock briefly to copy configuration, then unlock immediately
    // This minimizes the critical section and prevents blocking
    TaskConfig cfg;
    {
        std::lock_guard<std::mutex> lock(configMutex_);
        cfg = config_;
    }

    // ====== STEP 2: Get User Intent ======
    PlanResult intent = plan();

    // ====== STEP 3: Process Signal Channel ======
    processSignalChannel(cfg, intent.wantSignal);

    // ====== STEP 4: Process Action Channel ======
    processActionChannel(cfg, intent.wantAct);
}

void TaskBase::processSignalChannel(const TaskConfig& cfg, bool wantSignal) {
    // ====== Counter Management ======
    // Increment on "true", reset on "false" (immediate noise rejection)
    if (wantSignal) {
        sigCounter_++;
    } else {
        sigCounter_ = 0;
    }

    // ====== Condition Evaluation ======
    bool conditionMet = (sigCounter_ >= cfg.sigTolerance);
    bool gateOpen = cfg.allowSignal;

    // ====== State Machine Transitions ======
    
    // SCENARIO 1: WITHDRAWAL (Falling Edge or Gate Blocked)
    // User stopped asking OR gate closed AND we are currently signaled
    if ((!wantSignal || !gateOpen) && isSignaled_) {
        signal(false);           // Deactivate signal
        isSignaled_ = false;     // Update state
        sigCounter_ = 0;         // Reset counter
    }
    
    // SCENARIO 2: ACTIVATION (Rising Edge)
    // Condition met + Gate open + Currently NOT signaled
    else if (conditionMet && gateOpen && !isSignaled_) {
        signal(true);            // Activate signal
        isSignaled_ = true;      // Update state
    }
    
    // SCENARIO 3: REPEAT/HEARTBEAT (Steady State)
    // Condition met + Gate open + Currently signaled
    else if (conditionMet && gateOpen && isSignaled_) {
        // Check if repeat is configured
        if (cfg.sigRepeat > 0) {
            // Calculate delta past tolerance threshold
            int delta = sigCounter_ - cfg.sigTolerance;
            
            // Check if we've hit the repeat interval
            if (delta >= cfg.sigRepeat) {
                signal(true);                    // Re-fire signal (heartbeat)
                sigCounter_ = cfg.sigTolerance;  // SNAP-BACK: Reset to baseline
            }
        }
        // If sigRepeat == 0, single-shot behavior (no heartbeat)
    }
}

void TaskBase::processActionChannel(const TaskConfig& cfg, bool wantAct) {
    // ====== Counter Management ======
    if (wantAct) {
        actCounter_++;
    } else {
        actCounter_ = 0;
    }

    // ====== Condition Evaluation ======
    bool conditionMet = (actCounter_ >= cfg.actTolerance);
    bool gateOpen = cfg.allowAction;

    // ====== State Machine Transitions ======
    
    // SCENARIO 1: WITHDRAWAL (Falling Edge or Gate Blocked)
    if ((!wantAct || !gateOpen) && isActing_) {
        act(false);              // Deactivate action
        isActing_ = false;       // Update state
        actCounter_ = 0;         // Reset counter
    }
    
    // SCENARIO 2: ACTIVATION (Rising Edge)
    else if (conditionMet && gateOpen && !isActing_) {
        act(true);               // Activate action
        isActing_ = true;        // Update state
    }
    
    // SCENARIO 3: REPEAT/HEARTBEAT (Steady State)
    else if (conditionMet && gateOpen && isActing_) {
        if (cfg.actRepeat > 0) {
            int delta = actCounter_ - cfg.actTolerance;
            
            if (delta >= cfg.actRepeat) {
                act(true);                       // Re-fire action (heartbeat)
                actCounter_ = cfg.actTolerance;  // SNAP-BACK: Reset to baseline
            }
        }
    }
}

void TaskBase::updateConfig(int intervalMs,
                           int sigTolerance,
                           int sigRepeat,
                           bool allowSignal,
                           int actTolerance,
                           int actRepeat,
                           bool allowAction) {
    std::lock_guard<std::mutex> lock(configMutex_);
    config_.intervalMs = intervalMs;
    config_.sigTolerance = sigTolerance;
    config_.sigRepeat = sigRepeat;
    config_.allowSignal = allowSignal;
    config_.actTolerance = actTolerance;
    config_.actRepeat = actRepeat;
    config_.allowAction = allowAction;
}

void TaskBase::updateConfig(const TaskConfig& config) {
    std::lock_guard<std::mutex> lock(configMutex_);
    config_ = config;
}

int TaskBase::getInterval() const {
    std::lock_guard<std::mutex> lock(configMutex_);
    return config_.intervalMs;
}

} // namespace task_scheduler
