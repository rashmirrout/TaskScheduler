#include "scheduler.h"
#include "sensor_task.h"
#include "actuator_task.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <memory>

using namespace task_scheduler;

int main() {
    std::cout << "=================================================\n";
    std::cout << "  Thread-Safe Task Scheduling Framework Demo\n";
    std::cout << "=================================================\n\n";

    // Create scheduler with 4 worker threads
    Scheduler scheduler(4);

    std::cout << "1. SCOPE TEST: Creating tasks in local scope...\n";
    std::cout << "   (Tasks will persist even after scope ends)\n\n";

    // ====== SCOPE TEST ======
    {
        // Create SensorTask inside this scope
        scheduler.createTask("SensorA", []() {
            return std::make_shared<SensorTask>("SensorA", 1000, 50.0);
        });

        // Create ActuatorTask inside this scope
        scheduler.createTask("ActuatorA", []() {
            return std::make_shared<ActuatorTask>("ActuatorA", 800);
        });

        std::cout << "   Tasks created. Exiting scope in 2 seconds...\n";
        std::this_thread::sleep_for(std::chrono::seconds(2));
    } // <-- Scope ends here, but tasks are kept alive by scheduler registry

    std::cout << "\n   Scope ended. Tasks should still be running!\n";
    std::cout << "   Active tasks: " << scheduler.getTaskCount() << "\n\n";

    // Set sensor value above threshold to activate signal
    auto sensorTask = std::dynamic_pointer_cast<SensorTask>(scheduler.getTask("SensorA"));
    if (sensorTask) {
        sensorTask->setSensorValue(75.0);
    }

    // Enable actuator command
    auto actuatorTask = std::dynamic_pointer_cast<ActuatorTask>(scheduler.getTask("ActuatorA"));
    if (actuatorTask) {
        actuatorTask->setCommand(true);
    }

    std::cout << "2. OBSERVING: Tasks running at initial frequency...\n";
    std::cout << "   SensorA: 1000ms interval\n";
    std::cout << "   ActuatorA: 800ms interval\n\n";
    std::this_thread::sleep_for(std::chrono::seconds(3));

    // ====== DYNAMIC UPDATE TEST ======
    std::cout << "\n3. DYNAMIC UPDATE: Changing SensorA frequency to 200ms...\n\n";
    scheduler.updateTask("SensorA", 
                        200,    // intervalMs (was 1000ms)
                        10,     // sigTolerance
                        5,      // sigRepeat (heartbeat every 5 cycles)
                        true,   // allowSignal
                        10,     // actTolerance
                        0,      // actRepeat
                        true);  // allowAction

    std::cout << "   Updated! SensorA should now fire 5x faster...\n\n";
    std::this_thread::sleep_for(std::chrono::seconds(3));

    // ====== GATE TEST ======
    std::cout << "\n4. SAFETY GATE TEST: Disabling ActuatorA signal channel...\n\n";
    scheduler.updateTask("ActuatorA",
                        800,    // intervalMs
                        10,     // sigTolerance
                        0,      // sigRepeat
                        false,  // allowSignal <- GATE CLOSED
                        10,     // actTolerance
                        0,      // actRepeat
                        true);  // allowAction

    std::cout << "   Signal gate closed. ActuatorA signals should stop...\n\n";
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // ====== LAZY DELETION TEST ======
    std::cout << "\n5. LAZY DELETION TEST: Stopping SensorA...\n\n";
    scheduler.stopTask("SensorA");
    std::cout << "   SensorA stopped. Output should cease immediately.\n";
    std::cout << "   Active tasks: " << scheduler.getTaskCount() << "\n\n";
    std::this_thread::sleep_for(std::chrono::seconds(2));

    std::cout << "\n6. CLEAN SHUTDOWN: Stopping all remaining tasks...\n\n";
    scheduler.stopTask("ActuatorA");
    std::cout << "   Active tasks: " << scheduler.getTaskCount() << "\n\n";

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    std::cout << "=================================================\n";
    std::cout << "  Demo Complete - All tasks stopped\n";
    std::cout << "=================================================\n";

    // Scheduler destructor will clean up threads
    return 0;
}
