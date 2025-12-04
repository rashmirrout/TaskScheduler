#pragma once

#include "config_parser.h"
#include "task_base.h"
#include <memory>

namespace task_scheduler {

/**
 * @brief Factory for creating task instances from configurations
 * Supports creating SensorTask and ActuatorTask instances
 */
class TaskFactory {
public:
    /**
     * @brief Create a task instance from extended configuration
     * @param config Extended task configuration with type information
     * @return Shared pointer to created task, or nullptr if type is unknown
     */
    static std::shared_ptr<TaskBase> create(const ExtendedTaskConfig& config);
};

} // namespace task_scheduler
