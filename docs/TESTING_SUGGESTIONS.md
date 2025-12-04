# Testing Suggestions for TaskScheduler

## Overview

This document outlines additional tests that would enhance the test coverage and robustness of the TaskScheduler framework. The existing test suite covers core functionality well, but these suggestions target edge cases, integration scenarios, and configuration-driven features.

---

## Current Test Coverage

**Existing Test Files:**
1. `test_scheduler.cpp` - Scheduler functionality, timing, worker pool
2. `test_state_machine.cpp` - State machine logic, debouncing, heartbeats
3. `test_task_lifecycle.cpp` - Task creation, removal, persistence
4. `test_concurrency.cpp` - Multi-threaded operations, race conditions
5. `test_lazy_deletion.cpp` - Lazy deletion mechanism
6. `test_config_updates.cpp` - Dynamic configuration updates

---

## Suggested Additional Tests

### 1. Configuration Module Tests (`test_config_module.cpp`)

**Purpose**: Test the config module (parser, watcher, manager) in isolation and integration

#### Test Cases:

**ConfigParser Tests:**
```cpp
TEST(ConfigParserTest, ValidXMLParsing) {
    // Test parsing well-formed XML with all parameters
}

TEST(ConfigParserTest, MissingOptionalParameters) {
    // Verify defaults applied for missing sigRepeat, actRepeat, etc.
}

TEST(ConfigParserTest, InvalidTaskType) {
    // Test handling of unknown task types
}

TEST(ConfigParserTest, NegativeIntervalMs) {
    // Verify validation rejects negative intervals
}

TEST(ConfigParserTest, MalformedXML) {
    // Test error handling for syntax errors
}

TEST(ConfigParserTest, EmptyConfigFile) {
    // Verify behavior when no tasks defined
}

TEST(ConfigParserTest, DuplicateTaskNames) {
    // Test handling of duplicate task names in XML
}

TEST(ConfigParserTest, ExtremeTolerance Values) {
    // Test with tolerance = 0, 1, 1000, INT_MAX
}

TEST(ConfigParserTest, BooleanParsing) {
    // Test "true", "false", "1", "0", "yes", "no" variations
}
```

**FileWatcher Tests:**
```cpp
TEST(FileWatcherTest, DetectFileModification) {
    // Create file, start watcher, modify file, verify callback
}

TEST(FileWatcherTest, MultipleRapidChanges) {
    // Modify file multiple times quickly, verify all detected
}

TEST(FileWatcherTest, FileDeleted) {
    // Delete watched file, verify graceful handling
}

TEST(FileWatcherTest, FileRecreated) {
    // Delete then recreate file, verify detection
}

TEST(FileWatcherTest, StopWhileWatching) {
    // Call stop() while actively watching
}

TEST(FileWatcherTest, DoubleStart) {
    // Call start() twice, verify proper handling
}
```

**ConfigManager Tests:**
```cpp
TEST(ConfigManagerTest, InitialLoad) {
    // Start with valid config, verify tasks created
}

TEST(ConfigManagerTest, DebounceWindow) {
    // Modify file multiple times within debounce window
    // Verify only one update applied after window
}

TEST(ConfigManagerTest, AddTask) {
    // Add task to XML, verify it's created
}

TEST(ConfigManagerTest, RemoveTask) {
    // Remove task from XML, verify it's stopped
}

TEST(ConfigManagerTest, UpdateTask) {
    // Modify task parameters, verify update applied
}

TEST(ConfigManagerTest, MixedOperations) {
    // Add, update, remove in single config change
}

TEST(ConfigManagerTest, InvalidUpdateRollback) {
    // Update to invalid config, verify keeps old config
}

TEST(ConfigManagerTest, StopDuringDebounce) {
    // Stop manager while debounce window active
}
```

---

### 2. Module Integration Tests (`test_module_integration.cpp`)

**Purpose**: Test interaction between core, tasks, and config modules

#### Test Cases:

```cpp
TEST(ModuleIntegrationTest, CoreToTasks) {
    // Verify Scheduler correctly executes SensorTask and ActuatorTask
}

TEST(ModuleIntegrationTest, ConfigToCore) {
    // ConfigManager creates tasks in Scheduler, verify lifecycle
}

TEST(ModuleIntegrationTest, TaskFactoryIntegration) {
    // ConfigParser → TaskFactory → Scheduler flow
}

TEST(ModuleIntegrationTest, EndToEndConfigDriven) {
    // XML → Parser → Manager → Scheduler → Task execution
    // Full integration test
}

TEST(ModuleIntegrationTest, CrossModuleCommunication) {
    // Verify clean interfaces between modules
    // No unexpected dependencies
}
```

---

### 3. Edge Case Tests (`test_edge_cases.cpp`)

**Purpose**: Test boundary conditions and unusual scenarios

#### Test Cases:

**Timing Edge Cases:**
```cpp
TEST(EdgeCaseTest, ZeroInterval) {
    // Task with intervalMs = 0 (or reject in validation)
}

TEST(EdgeCaseTest, VeryLongInterval) {
    // intervalMs = INT_MAX
}

TEST(EdgeCaseTest, VeryShortInterval) {
    // intervalMs = 1ms (stress test)
}

TEST(EdgeCaseTest, TaskCompletesBeforeReschedule) {
    // run() takes longer than interval
}
```

**Counter Edge Cases:**
```cpp
TEST(EdgeCaseTest, CounterOverflow) {
    // Run task long enough for counter to approach INT_MAX
}

TEST(EdgeCaseTest, ZeroTolerance) {
    // tolerance = 0 (immediate activation)
}

TEST(EdgeCaseTest, ToleranceOne) {
    // tolerance = 1 (minimal debounce)
}

TEST(EdgeCaseTest, RepeatGreaterThanTolerance) {
    // repeat > tolerance (invalid, or special behavior?)
}
```

**State Transition Edge Cases:**
```cpp
TEST(EdgeCaseTest, RapidPlanFlips) {
    // plan() alternates true/false every cycle
}

TEST(EdgeCaseTest, GateCloseDuringActivation) {
    // Close gate exactly when counter == tolerance
}

TEST(EdgeCaseTest, GateToggling) {
    // Rapidly open/close gate
}

TEST(EdgeCaseTest, SimultaneousChannelActivation) {
    // Both signal and action activate same cycle
}
```

**Resource Edge Cases:**
```cpp
TEST(EdgeCaseTest, ThousandTasks) {
    // Create 1000+ tasks, verify performance
}

TEST(EdgeCaseTest, RapidCreateDestroy) {
    // Create and destroy tasks in tight loop
}

TEST(EdgeCaseTest, SingleWorkerThread) {
    // Scheduler with only 1 worker
}

TEST(EdgeCaseTest, ManyWorkerThreads) {
    // Scheduler with 100 workers
}
```

---

### 4. Error Handling Tests (`test_error_handling.cpp`)

**Purpose**: Verify graceful degradation and error recovery

#### Test Cases:

```cpp
TEST(ErrorHandlingTest, NullTaskFactory) {
    // Factory returns nullptr, verify handling
}

TEST(ErrorHandlingTest, TaskThrowsInPlan) {
    // plan() throws exception, verify isolation
}

TEST(ErrorHandlingTest, TaskThrowsInSignal) {
    // signal() throws exception, verify recovery
}

TEST(ErrorHandlingTest, TaskThrowsInAct) {
    // act() throws exception, verify recovery
}

TEST(ErrorHandlingTest, InvalidConfigUpdate) {
    // Update with invalid parameters, verify rejection
}

TEST(ErrorHandlingTest, FileWatcherIOError) {
    // Simulate filesystem errors
}

TEST(ErrorHandlingTest, SchedulerShutdownDuringExecution) {
    // Shutdown while tasks running
}

TEST(ErrorHandlingTest, OutOfMemory Simulation) {
    // Simulate allocation failures (if possible)
}
```

---

### 5. Performance Tests (`test_performance.cpp`)

**Purpose**: Measure and validate performance characteristics

#### Test Cases:

```cpp
TEST(PerformanceTest, SchedulingOverhead) {
    // Measure time between scheduled and actual execution
}

TEST(PerformanceTest, ThroatoughputSingleWorker) {
    // Tasks/second with 1 worker
}

TEST(PerformanceTest, ThroughputMultipleWorkers) {
    // Tasks/second scaling with N workers
}

TEST(PerformanceTest, ConfigUpdateLatency) {
    // Time from config change to task update
}

TEST(PerformanceTest, MemoryUsage) {
    // Memory footprint with N tasks
}

TEST(PerformanceTest, CPUUsageIdle) {
    // CPU when no tasks scheduled (should be ~0%)
}

TEST(PerformanceTest, LockContention) {
    // Measure mutex wait times under load
}

TEST(PerformanceTest, CacheEfficiency) {
    // Benchmark with varying task counts
}
```

---

### 6. Real-World Scenario Tests (`test_scenarios.cpp`)

**Purpose**: Test common usage patterns and scenarios

#### Test Cases:

```cpp
TEST(ScenarioTest, SensorMonitoringSystem) {
    // Multiple sensors, threshold alerts, recovery
}

TEST(ScenarioTest, ControlLoop) {
    // PID-like control with feedback
}

TEST(ScenarioTest, HeartbeatMonitoring) {
    // Tasks with repeat intervals, missed heartbeats
}

TEST(ScenarioTest, BatchProcessing) {
    // Tasks trigger at specific times
}

TEST(ScenarioTest, CircuitBreaker Pattern) {
    // Task activates/deactivates based on failure rate
}

TEST(ScenarioTest, GracefulDegradation) {
    // System behavior when resources constrained
}

TEST(ScenarioTest, HotReloadWorkflow) {
    // Edit config, verify changes applied without restart
}
```

---

### 7. Thread Safety Tests (`test_thread_safety.cpp`)

**Purpose**: Additional concurrency tests beyond existing suite

#### Test Cases:

```cpp
TEST(ThreadSafetyTest, ConfigUpdateDuringRun) {
    // Update config while task executing run()
}

TEST(ThreadSafetyTest, CreateDuringShutdown) {
    // Create task while shutdown in progress
}

TEST(ThreadSafetyTest, RaceOnStopAndExecute) {
    // stopTask() called as task about to execute
}

TEST(ThreadSafetyTest, MultipleConfigManagers) {
    // Two ConfigManagers on same Scheduler (if supported)
}

TEST(ThreadSafetyTest, ThreadSanitizer) {
    // Run under ThreadSanitizer, verify no races
}

TEST(ThreadSafetyTest, StressTestAllOperations) {
    // Random mix of create, update, stop, execute
}
```

---

### 8. Regression Tests (`test_regression.cpp`)

**Purpose**: Tests for specific bugs found and fixed

#### Example Structure:

```cpp
TEST(RegressionTest, Issue1_CounterOverflowOnLongRun) {
    // Bug: Counter overflow after running for hours
    // Fix: Implemented snap-back mechanism
}

TEST(RegressionTest, Issue2_RaceInLazyDeletion) {
    // Bug: Rare crash when stopping task
    // Fix: Added atomic active flag
}

// Add tests for each bug found
```

---

## Test Utilities and Helpers

### Suggested Test Utilities:

```cpp
// Mock task for flexible testing
class MockTask : public TaskBase {
    std::function<PlanResult()> planFunc_;
    std::function<void(bool)> signalFunc_;
    std::function<void(bool)> actFunc_;
    
public:
    void setPlanBehavior(std::function<PlanResult()> func);
    void setSignalBehavior(std::function<void(bool)> func);
    void setActBehavior(std::function<void(bool)> func);
};

// XML builder for tests
class TestXMLBuilder {
public:
    TestXMLBuilder& addTask(const ExtendedTaskConfig& config);
    std::string build();
    void writeToFile(const std::string& path);
};

// Test fixture with common setup
class SchedulerTestFixture : public ::testing::Test {
protected:
    std::unique_ptr<Scheduler> scheduler;
    std::string testConfigPath;
    
    void SetUp() override;
    void TearDown() override;
    
    // Helper methods
    void waitForTaskExecution(const std::string& taskName, int cycles);
    void verifyTaskState(const std::string& taskName, bool expected);
};
```

---

## Code Coverage Goals

**Target Coverage:**
- Line Coverage: >95%
- Branch Coverage: >90%
- Function Coverage: 100%

**Tools:**
- gcov/lcov for coverage analysis
- gcovr for reports
- CI/CD integration for continuous monitoring

---

## Test Execution Strategy

### Unit Tests
- Run on every commit
- Fast execution (<5 seconds total)
- Isolated, deterministic

### Integration Tests
- Run on pull requests
- Moderate execution time (<30 seconds)
- Test module interactions

### Performance Tests
- Run nightly or on demand
- Longer execution time (minutes)
- Establish baselines, detect regressions

### Stress Tests
- Run weekly or manually
- Very long execution (hours)
- Memory leaks, thread safety over time

---

## Continuous Integration

**Recommended CI Pipeline:**

```yaml
# Example GitHub Actions workflow
name: Tests

on: [push, pull_request]

jobs:
  unit-tests:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v2
      - name: Build
        run: cmake -B build && cmake --build build
      - name: Run Tests
        run: cd build && ctest --output-on-failure
      
  coverage:
    runs-on: ubuntu-latest
    steps:
      - name: Generate Coverage
        run: cmake -DCMAKE_BUILD_TYPE=Coverage ...
      - name: Upload Coverage
        uses: codecov/codecov-action@v2
  
  sanitizers:
    runs-on: ubuntu-latest
    strategy:
      matrix:
        sanitizer: [address, thread, undefined]
    steps:
      - name: Build with ${{ matrix.sanitizer }}
        run: cmake -DCMAKE_CXX_FLAGS="-fsanitize=${{ matrix.sanitizer }}" ...
```

---

## Priority Recommendations

### High Priority (Implement First):
1. ✅ Configuration module tests - Essential for config-driven feature
2. ✅ Edge case tests - Catches boundary condition bugs
3. ✅ Error handling tests - Ensures robustness

### Medium Priority:
4. ⚠️ Module integration tests - Validates clean architecture
5. ⚠️ Performance tests - Establishes baselines
6. ⚠️ Real-world scenarios - Validates practical usage

### Low Priority (Nice to Have):
7. 📝 Thread safety stress tests - Already well covered
8. 📝 Regression tests - Add as bugs discovered

---

## Metrics to Track

**Test Metrics:**
- Total test count
- Pass/fail rate
- Execution time
- Code coverage %
- Flaky test rate

**Quality Metrics:**
- Bugs found by tests
- Bugs missed by tests (found in production)
- Mean time to detect bugs

---

## Conclusion

Implementing these additional tests will:
1. **Increase confidence** in the framework's robustness
2. **Catch edge cases** before they become production issues
3. **Document behavior** through executable specifications
4. **Enable refactoring** with safety net
5. **Validate architecture** through integration tests

The modular structure makes it easy to add these tests incrementally, focusing first on high-priority areas.

---

*Document Version: 1.0*  
*Last Updated: 2025-12-04*
