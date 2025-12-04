#include "core/scheduler.h"
#include <algorithm>

namespace task_scheduler {

Scheduler::Scheduler(size_t numWorkers)
    : running_(true)
{
    // Start timer thread
    timerThread_ = std::thread(&Scheduler::timerThreadFunc, this);

    // Start worker threads
    workerThreads_.reserve(numWorkers);
    for (size_t i = 0; i < numWorkers; ++i) {
        workerThreads_.emplace_back(&Scheduler::workerThreadFunc, this);
    }
}

Scheduler::~Scheduler() {
    shutdown();
}

void Scheduler::shutdown() {
    if (!running_.exchange(false)) {
        return; // Already shut down
    }

    // Wake up all threads
    timerCV_.notify_all();
    workerCV_.notify_all();

    // Wait for timer thread
    if (timerThread_.joinable()) {
        timerThread_.join();
    }

    // Wait for worker threads
    for (auto& thread : workerThreads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
}

bool Scheduler::stopTask(const std::string& name) {
    std::lock_guard<std::mutex> lock(registryMutex_);
    
    auto it = registry_.find(name);
    if (it == registry_.end()) {
        return false;
    }

    // Mark task as inactive (lazy deletion - will be dropped when popped)
    it->second->setActive(false);

    // Remove from registry (releases ownership)
    registry_.erase(it);

    return true;
}

bool Scheduler::updateTask(const std::string& name,
                          int intervalMs,
                          int sigTolerance,
                          int sigRepeat,
                          bool allowSignal,
                          int actTolerance,
                          int actRepeat,
                          bool allowAction) {
    TaskConfig config;
    config.intervalMs = intervalMs;
    config.sigTolerance = sigTolerance;
    config.sigRepeat = sigRepeat;
    config.allowSignal = allowSignal;
    config.actTolerance = actTolerance;
    config.actRepeat = actRepeat;
    config.allowAction = allowAction;
    
    return updateTask(name, config);
}

bool Scheduler::updateTask(const std::string& name, const TaskConfig& config) {
    std::lock_guard<std::mutex> lock(registryMutex_);
    
    auto it = registry_.find(name);
    if (it == registry_.end()) {
        return false;
    }

    it->second->updateConfig(config);
    return true;
}

std::shared_ptr<TaskBase> Scheduler::getTask(const std::string& name) {
    std::lock_guard<std::mutex> lock(registryMutex_);
    
    auto it = registry_.find(name);
    if (it == registry_.end()) {
        return nullptr;
    }
    
    return it->second;
}

size_t Scheduler::getTaskCount() const {
    std::lock_guard<std::mutex> lock(registryMutex_);
    return registry_.size();
}

void Scheduler::scheduleTask(std::shared_ptr<TaskBase> task) {
    if (!task) {
        return;
    }

    auto now = std::chrono::steady_clock::now();
    auto interval = std::chrono::milliseconds(task->getInterval());
    auto nextRun = now + interval;

    {
        std::lock_guard<std::mutex> lock(timerMutex_);
        timerQueue_.push({nextRun, task});
    }
    
    // Wake up timer thread to recalculate sleep time
    timerCV_.notify_one();
}

void Scheduler::rescheduleTask(std::shared_ptr<TaskBase> task) {
    scheduleTask(task);
}

void Scheduler::timerThreadFunc() {
    while (running_.load()) {
        std::unique_lock<std::mutex> lock(timerMutex_);

        // Wait until we have tasks or are shutting down
        if (timerQueue_.empty()) {
            timerCV_.wait(lock, [this] {
                return !timerQueue_.empty() || !running_.load();
            });
            
            if (!running_.load()) {
                break;
            }
        }

        // Get the earliest deadline
        auto now = std::chrono::steady_clock::now();
        auto& entry = timerQueue_.top();

        if (entry.nextRunTime <= now) {
            // Time to execute - move to worker queue
            auto task = entry.task;
            timerQueue_.pop();
            lock.unlock();

            // Check if task is still active (lazy deletion)
            if (task->isActive()) {
                // Add to worker queue
                {
                    std::lock_guard<std::mutex> workerLock(workerMutex_);
                    workerQueue_.push(task);
                }
                workerCV_.notify_one();
            }
            // If inactive, just drop it (lazy deletion complete)
        } else {
            // Sleep until next deadline
            auto sleepTime = entry.nextRunTime - now;
            timerCV_.wait_for(lock, sleepTime, [this, &entry] {
                // Wake up if shutdown or queue changed (new earlier task)
                return !running_.load() || 
                       (timerQueue_.empty() ? false : timerQueue_.top().nextRunTime < entry.nextRunTime);
            });
        }
    }
}

void Scheduler::workerThreadFunc() {
    while (running_.load()) {
        std::shared_ptr<TaskBase> task;

        {
            std::unique_lock<std::mutex> lock(workerMutex_);
            
            // Wait for work or shutdown
            workerCV_.wait(lock, [this] {
                return !workerQueue_.empty() || !running_.load();
            });

            if (!running_.load() && workerQueue_.empty()) {
                break;
            }

            if (!workerQueue_.empty()) {
                task = workerQueue_.front();
                workerQueue_.pop();
            }
        }

        if (task) {
            // Check if still active (lazy deletion)
            if (task->isActive()) {
                // Execute the task
                task->run();

                // Reschedule if still active after execution
                if (task->isActive()) {
                    rescheduleTask(task);
                }
            }
            // If inactive, drop it (lazy deletion complete)
        }
    }
}

} // namespace task_scheduler
