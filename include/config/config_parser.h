#pragma once

#include "core/types.h"
#include <string>
#include <vector>

namespace task_scheduler {

/**
 * @brief Extended task configuration with type information
 * Contains TaskConfig plus task type (SensorTask, ActuatorTask, etc.)
 */
struct ExtendedTaskConfig {
    TaskConfig config;
    std::string taskType;  // "SensorTask" or "ActuatorTask"
    
    bool operator==(const ExtendedTaskConfig& other) const {
        return config.taskName == other.config.taskName &&
               config.intervalMs == other.config.intervalMs &&
               config.sigTolerance == other.config.sigTolerance &&
               config.sigRepeat == other.config.sigRepeat &&
               config.allowSignal == other.config.allowSignal &&
               config.actTolerance == other.config.actTolerance &&
               config.actRepeat == other.config.actRepeat &&
               config.allowAction == other.config.allowAction &&
               taskType == other.taskType;
    }
    
    bool operator!=(const ExtendedTaskConfig& other) const {
        return !(*this == other);
    }
};

/**
 * @brief XML configuration parser for task configurations
 * Reads XML file and converts to ExtendedTaskConfig structures
 */
class ConfigParser {
public:
    /**
     * @brief Parse XML configuration file
     * @param xmlPath Path to XML configuration file
     * @return Vector of task configurations, empty on parse failure
     */
    static std::vector<ExtendedTaskConfig> parse(const std::string& xmlPath);
    
private:
    /**
     * @brief Validate a single task configuration
     * @param config Configuration to validate
     * @return true if valid, false otherwise
     */
    static bool validateConfig(const ExtendedTaskConfig& config);
    
    /**
     * @brief Parse boolean value from string
     * @param value String value ("true", "false", "1", "0")
     * @return Boolean value
     */
    static bool parseBool(const std::string& value);
};

} // namespace task_scheduler
