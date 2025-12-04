#include "tasks/task_factory.h"
#include "tasks/sensor_task.h"
#include "tasks/actuator_task.h"
#include <iostream>

namespace task_scheduler {

std::shared_ptr<TaskBase> TaskFactory::create(const ExtendedTaskConfig& config) {
    if (config.taskType == "SensorTask") {
        // Create SensorTask with default threshold (50.0)
        // In future, threshold could be added to XML if needed
        return std::make_shared<SensorTask>(config.config, 50.0);
    } 
    else if (config.taskType == "ActuatorTask") {
        return std::make_shared<ActuatorTask>(config.config);
    }
    else {
        std::cerr << "TaskFactory: Unknown task type '" << config.taskType 
                  << "' for task '" << config.config.taskName << "'" << std::endl;
        return nullptr;
    }
}

} // namespace task_scheduler
