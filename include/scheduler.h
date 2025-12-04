#pragma once

#include "types.h"
#include "task_base.h"
#include <string>
#include <memory>
#include <unordered_map>
#include <queue>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <functional>

namespace task_scheduler {

/**
 * @brief Thread-safe task scheduler with priority queue and worker thread pool
 * 
 * Architecture:
 * - Registry: Keeps tasks alive via shared_ptr (tasks persist across scope boundaries)
 * - Timer Thread: Manages priority queue, moves tasks to worker queue at scheduled time
 * - Worker Threads: Execute task->run() and reschedule if still active
 * - Lazy Deletion: Inactive tasks are dropped when naturally popped from queues
 */
class Scheduler {
public:
    /**
     * @brief Constructor
     * @param numWorkers Number of worker threads to create
     */
    explicit Scheduler(size_t numWorkers = 4);

    /**
     * @brief Destructor - stops all threads and cleans up
     */
    ~Scheduler();

    // Prevent copying
    Scheduler(const Scheduler&) = delete;
    Scheduler& operator=(const Scheduler&) = delete;

    // ====== Task Management ======
    /**
     * @brief Create and register a task using a factory function
     * @param name Unique task identifier
     * @param factory Function that creates the task
     * @return True if created successfully, false if name already exists
     */
    template<typename Factory>
    bool createTask(const std::string& name, Factory&& factory) {
        std::lock_guard<std::mutex> lock(registryMutex_);
        
        // Check if task already exists
        if (registry_.find(name) != registry_.end()) {
            return false;
        }
        
        // Create task
        auto task = factory();
        if (!task) {
            return false;
        }
        
        // Register and schedule
        registry_[name] = task;
        scheduleTask(task);
        
        return true;
    }

    /**
     * @brief Stop a task (sets inactive, removes from registry)
     * Task will be dropped when naturally popped from queues (lazy deletion)
     * @param name Task identifier
     * @return True if task was found and stopped
     */
    bool stopTask(const std::string& name);

    /**
     * @brief Update task configuration (individual parameters)
     * @param name Task identifier
     * @return True if task was found
     */
    bool updateTask(const std::string& name,
                   int intervalMs,
                   int sigTolerance,
                   int sigRepeat,
                   bool allowSignal,
                   int actTolerance,
                   int actRepeat,
                   bool allowAction);

    /**
     * @brief Update task configuration (using TaskConfig struct)
     * @param name Task identifier
     * @param config Configuration struct
     * @return True if task was found
     */
    bool updateTask(const std::string& name, const TaskConfig& config);

    /**
     * @brief Get task by name (for testing/inspection)
     * @param name Task identifier
     * @return Shared pointer to task, or nullptr if not found
     */
    std::shared_ptr<TaskBase> getTask(const std::string& name);

    /**
     * @brief Get number of registered tasks
     */
    size_t getTaskCount() const;

    /**
     * @brief Shutdown scheduler (stop all threads)
     */
    void shutdown();

private:
    // ====== Internal Scheduling ======
    void scheduleTask(std::shared_ptr<TaskBase> task);
    void rescheduleTask(std::shared_ptr<TaskBase> task);

    // ====== Thread Entry Points ======
    void timerThreadFunc();
    void workerThreadFunc();

    // ====== Registry (Task Ownership) ======
    std::unordered_map<std::string, std::shared_ptr<TaskBase>> registry_;
    mutable std::mutex registryMutex_;

    // ====== Timer Queue (Priority Queue) ======
    std::priority_queue<ScheduleEntry, std::vector<ScheduleEntry>, std::greater<ScheduleEntry>> timerQueue_;
    std::mutex timerMutex_;
    std::condition_variable timerCV_;

    // ====== Worker Queue (FIFO) ======
    std::queue<std::shared_ptr<TaskBase>> workerQueue_;
    std::mutex workerMutex_;
    std::condition_variable workerCV_;

    // ====== Thread Management ======
    std::thread timerThread_;
    std::vector<std::thread> workerThreads_;
    std::atomic<bool> running_;
};

} // namespace task_scheduler
