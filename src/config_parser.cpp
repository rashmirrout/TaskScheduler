#include "config_parser.h"
#include <pugixml.hpp>
#include <iostream>
#include <algorithm>

namespace task_scheduler {

std::vector<ExtendedTaskConfig> ConfigParser::parse(const std::string& xmlPath) {
    std::vector<ExtendedTaskConfig> configs;
    
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_file(xmlPath.c_str());
    
    if (!result) {
        std::cerr << "XML Parse Error: " << result.description() 
                  << " at offset " << result.offset << std::endl;
        return configs;  // Return empty vector on error
    }
    
    pugi::xml_node root = doc.child("TaskConfigurations");
    if (!root) {
        std::cerr << "XML Error: Root element 'TaskConfigurations' not found" << std::endl;
        return configs;
    }
    
    // Parse each Task element
    for (pugi::xml_node taskNode : root.children("Task")) {
        ExtendedTaskConfig extConfig;
        
        try {
            // Parse required fields
            std::string taskName = taskNode.child_value("taskName");
            std::string taskType = taskNode.child_value("taskType");
            
            if (taskName.empty() || taskType.empty()) {
                std::cerr << "XML Error: Task missing required fields (taskName or taskType)" << std::endl;
                continue;
            }
            
            // Parse TaskConfig fields
            extConfig.config.taskName = taskName;
            extConfig.taskType = taskType;
            
            // Parse numeric and boolean fields with defaults
            extConfig.config.intervalMs = taskNode.child("intervalMs").text().as_int(1000);
            extConfig.config.sigTolerance = taskNode.child("sigTolerance").text().as_int(10);
            extConfig.config.sigRepeat = taskNode.child("sigRepeat").text().as_int(0);
            extConfig.config.actTolerance = taskNode.child("actTolerance").text().as_int(10);
            extConfig.config.actRepeat = taskNode.child("actRepeat").text().as_int(0);
            
            // Parse boolean fields
            std::string allowSignal = taskNode.child_value("allowSignal");
            std::string allowAction = taskNode.child_value("allowAction");
            
            extConfig.config.allowSignal = allowSignal.empty() ? true : parseBool(allowSignal);
            extConfig.config.allowAction = allowAction.empty() ? true : parseBool(allowAction);
            
            // Validate configuration
            if (!validateConfig(extConfig)) {
                std::cerr << "XML Error: Invalid configuration for task: " << taskName << std::endl;
                continue;
            }
            
            configs.push_back(extConfig);
            
        } catch (const std::exception& e) {
            std::cerr << "XML Parse Exception: " << e.what() << std::endl;
            continue;
        }
    }
    
    if (configs.empty()) {
        std::cerr << "XML Warning: No valid tasks found in configuration file" << std::endl;
    } else {
        std::cout << "Successfully parsed " << configs.size() << " task(s) from " << xmlPath << std::endl;
    }
    
    return configs;
}

bool ConfigParser::validateConfig(const ExtendedTaskConfig& config) {
    // Validate task name
    if (config.config.taskName.empty()) {
        return false;
    }
    
    // Validate task type
    if (config.taskType != "SensorTask" && config.taskType != "ActuatorTask") {
        std::cerr << "Validation Error: Invalid task type '" << config.taskType 
                  << "' for task '" << config.config.taskName << "'" << std::endl;
        return false;
    }
    
    // Validate interval (must be positive)
    if (config.config.intervalMs <= 0) {
        std::cerr << "Validation Error: Invalid intervalMs (" << config.config.intervalMs 
                  << ") for task '" << config.config.taskName << "'" << std::endl;
        return false;
    }
    
    // Validate tolerance values (must be non-negative)
    if (config.config.sigTolerance < 0 || config.config.actTolerance < 0) {
        std::cerr << "Validation Error: Tolerance values must be non-negative for task '" 
                  << config.config.taskName << "'" << std::endl;
        return false;
    }
    
    // Validate repeat values (must be non-negative)
    if (config.config.sigRepeat < 0 || config.config.actRepeat < 0) {
        std::cerr << "Validation Error: Repeat values must be non-negative for task '" 
                  << config.config.taskName << "'" << std::endl;
        return false;
    }
    
    return true;
}

bool ConfigParser::parseBool(const std::string& value) {
    std::string lowerValue = value;
    std::transform(lowerValue.begin(), lowerValue.end(), lowerValue.begin(), ::tolower);
    
    return (lowerValue == "true" || lowerValue == "1" || lowerValue == "yes");
}

} // namespace task_scheduler
