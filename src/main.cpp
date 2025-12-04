#include "core/scheduler.h"
#include "config/config_manager.h"
#include "tasks/sensor_task.h"
#include "tasks/actuator_task.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <memory>
#include <filesystem>

using namespace task_scheduler;

int main() {
    std::cout << "==========================================================\n";
    std::cout << "  Config-Driven Task Scheduling Framework Demo\n";
    std::cout << "==========================================================\n\n";

    // Create scheduler with 4 worker threads
    Scheduler scheduler(4);

    std::cout << "1. MANUAL TASK CREATION (Demo)\n";
    std::cout << "   Creating a demo task programmatically...\n\n";

    // Create a demo task manually to show both approaches work together
    scheduler.createTask("DemoTask", []() {
        return std::make_shared<SensorTask>(
            TaskConfig{"DemoTask", 2000, 10, 0, true, 10, 0, true},
            50.0
        );
    });

    std::cout << "   Demo task created: DemoTask (2000ms interval)\n";
    std::cout << "   Active tasks: " << scheduler.getTaskCount() << "\n\n";

    // Let demo task run briefly
    std::this_thread::sleep_for(std::chrono::seconds(3));

    // Path to configuration file
    std::string configPath = "config/tasks.xml";
    
    // Check if config file exists
    if (!std::filesystem::exists(configPath)) {
        std::cerr << "Error: Configuration file not found: " << configPath << std::endl;
        std::cerr << "Please create the config file or update the path." << std::endl;
        return 1;
    }

    std::cout << "\n2. CONFIG-DRIVEN INITIALIZATION\n";
    std::cout << "   Loading tasks from: " << configPath << "\n\n";

    // Create ConfigManager with 1-minute debounce for testing (default is 5 min)
    ConfigManager configManager(scheduler, configPath, std::chrono::minutes(1));
    
    if (!configManager.start()) {
        std::cerr << "Failed to start ConfigManager" << std::endl;
        return 1;
    }

    std::cout << "\n3. COMBINED TASKS RUNNING\n";
    std::cout << "   Manual task: DemoTask (2000ms)\n";
    std::cout << "   Config tasks: " << (scheduler.getTaskCount() - 1) << " task(s)\n";
    std::cout << "   Total active tasks: " << scheduler.getTaskCount() << "\n";
    std::cout << "   File watcher is monitoring: " << configPath << "\n\n";

    // Let tasks run for observation
    std::cout << "   Letting tasks run for 5 seconds...\n";
    std::this_thread::sleep_for(std::chrono::seconds(5));

    std::cout << "\n4. FILE WATCHING DEMONSTRATION\n";
    std::cout << "   The system is now watching for configuration changes.\n";
    std::cout << "   You can modify " << configPath << " to:\n";
    std::cout << "   - Add new tasks\n";
    std::cout << "   - Update existing task configurations\n";
    std::cout << "   - Remove tasks\n";
    std::cout << "   \n";
    std::cout << "   Changes will be applied after 1-minute debounce window.\n";
    std::cout << "   \n";
    std::cout << "   Press Ctrl+C to exit, or wait 30 seconds for auto-shutdown...\n\n";

    // Run for 30 seconds to allow testing file changes
    for (int i = 30; i > 0; i--) {
        std::cout << "   Remaining: " << i << " seconds   \r" << std::flush;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    std::cout << "\n\n5. CLEAN SHUTDOWN\n";
    std::cout << "   Stopping ConfigManager...\n";
    configManager.stop();
    
    std::cout << "   Final task count: " << scheduler.getTaskCount() << "\n\n";

    std::cout << "==========================================================\n";
    std::cout << "  Demo Complete - Config-Driven System Demonstrated\n";
    std::cout << "==========================================================\n";

    // Scheduler destructor will clean up threads
    return 0;
}
