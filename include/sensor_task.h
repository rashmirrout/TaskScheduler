#pragma once

#include "task_base.h"
#include <atomic>

namespace task_scheduler {

/**
 * @brief Concrete task simulating a sensor reading
 * 
 * Demonstrates signal channel usage with configurable threshold
 */
class SensorTask : public TaskBase {
public:
    /**
     * @brief Constructor
     * @param name Task identifier
     * @param intervalMs Execution interval
     * @param threshold Sensor reading threshold for activation
     */
    SensorTask(const std::string& name, 
               int intervalMs,
               double threshold = 50.0);

    // Override abstract methods
    PlanResult plan() override;
    void signal(bool doSignal) override;
    void act(bool doAct) override;

    // ====== Test/Demo Interface ======
    /**
     * @brief Set simulated sensor value (for testing/demo)
     */
    void setSensorValue(double value) { sensorValue_.store(value); }

    /**
     * @brief Get current sensor value
     */
    double getSensorValue() const { return sensorValue_.load(); }

    /**
     * @brief Set threshold
     */
    void setThreshold(double threshold) { threshold_ = threshold; }

private:
    std::atomic<double> sensorValue_;  // Simulated sensor reading
    double threshold_;                  // Activation threshold
};

} // namespace task_scheduler
