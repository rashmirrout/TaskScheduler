# TaskScheduler - Thread-Safe Task Scheduling Framework

A production-ready C++17 task scheduling framework featuring sophisticated state machine logic, thread-safe execution, and comprehensive testing.

## Features

### Core Architecture
- **Priority Queue Scheduler** with worker thread pool
- **Lazy Deletion** for efficient task lifecycle management
- **Registry-based Ownership** - tasks persist across scope boundaries
- **Thread-Safe Configuration** with snapshot-based concurrency control

### State Machine
- **Debouncing** - Tolerance-based noise filtering
- **Heartbeat/Repeat** - Configurable periodic re-firing with snap-back logic
- **Safety Gates** - Global enable/disable for signal and action channels
- **Independent Channels** - Signal and Action channels operate independently

### Quality
- **100% Thread-Safe** - All operations protected by appropriate synchronization
- **Comprehensive Test Suite** - 70+ unit tests covering all functionality
- **Zero Busy-Waiting** - Efficient condition variable-based scheduling
- **Memory Safe** - Smart pointer-based ownership with verified cleanup

## Project Structure

The project follows a modular architecture with clear separation of concerns:

```
TaskScheduler/
├── CMakeLists.txt              # Root CMake configuration
├── README.md                   # This file
├── LICENSE                     # License file
├── .gitignore                  # Git ignore patterns
│
├── include/                    # Public headers (modular structure)
│   ├── core/                   # Core scheduling engine
│   │   ├── types.h            # Common data structures (TaskConfig, PlanResult)
│   │   ├── task_base.h        # Abstract task interface & state machine
│   │   └── scheduler.h        # Thread-safe scheduler with priority queue
│   ├── tasks/                  # Concrete task implementations
│   │   ├── sensor_task.h      # Example sensor task (signal-focused)
│   │   ├── actuator_task.h    # Example actuator task (action-focused)
│   │   └── task_factory.h     # Factory pattern for task creation
│   └── config/                 # Configuration management
│       ├── config_parser.h    # XML configuration parser
│       ├── file_watcher.h     # File system monitoring
│       └── config_manager.h   # Hot-reload & task synchronization
│
├── src/                        # Implementation files (mirrors include structure)
│   ├── CMakeLists.txt         # Library build configuration
│   ├── core/                   # Core implementations
│   │   ├── task_base.cpp      # State machine logic
│   │   └── scheduler.cpp      # Scheduler implementation
│   ├── tasks/                  # Task implementations
│   │   ├── sensor_task.cpp    # Sensor task implementation
│   │   ├── actuator_task.cpp  # Actuator task implementation
│   │   └── task_factory.cpp   # Factory implementation
│   ├── config/                 # Config implementations
│   │   ├── config_parser.cpp  # XML parsing with pugixml
│   │   ├── file_watcher.cpp   # Cross-platform file watching
│   │   └── config_manager.cpp # Config lifecycle management
│   └── main.cpp               # Demo application
│
├── config/                     # Configuration files
│   └── tasks.xml              # Example task configuration
│
├── tests/                      # Comprehensive test suite
│   ├── CMakeLists.txt         # Test build configuration
│   ├── test_main.cpp          # GTest entry point
│   ├── test_scheduler.cpp     # Scheduler functionality tests
│   ├── test_state_machine.cpp # State machine logic tests
│   ├── test_task_lifecycle.cpp # Task creation/deletion tests
│   ├── test_config_updates.cpp # Dynamic config update tests
│   ├── test_concurrency.cpp   # Thread safety & race condition tests
│   └── test_lazy_deletion.cpp # Lazy deletion mechanism tests
│
└── docs/                       # Documentation
    ├── ARCHITECTURE.md        # Detailed architecture documentation
    ├── CONFIG_DRIVEN.md       # Config-driven feature documentation
    └── TESTING_SUGGESTIONS.md # Additional test recommendations
```

### Module Organization

**Core Module (`core/`):**
- Foundation of the framework - "the brain and center"
- `task_base`: Template Method pattern, state machine implementation
- `scheduler`: Thread pool, priority queue, task lifecycle
- `types`: Shared data structures

**Tasks Module (`tasks/`):**
- Concrete task implementations demonstrating framework usage
- `sensor_task`: Signal-focused monitoring task
- `actuator_task`: Action-focused control task
- `task_factory`: Creates tasks from configuration

**Config Module (`config/`):**
- Configuration-driven task management
- `config_parser`: XML parsing and validation
- `file_watcher`: Monitors configuration file changes
- `config_manager`: Hot-reload with debouncing, task synchronization

## Requirements

- **Compiler**: C++17 compatible compiler
  - GCC 7+ / Clang 5+ / MSVC 2017+
- **CMake**: 3.14 or higher
- **Operating System**: Windows, Linux, macOS
- **Dependencies**: All dependencies are automatically fetched by CMake
  - GoogleTest 1.14.0 (testing framework)
  - pugixml 1.14 (XML parsing library)
  - **No git submodules or manual installation required!**

## Building

### First-Time Build (Internet Required)

When you run CMake for the first time, it will automatically download all dependencies:

```bash
# Clone the repository
git clone https://github.com/rashmirrout/TaskScheduler.git
cd TaskScheduler

# Create build directory
mkdir build
cd build

# Configure with CMake (downloads dependencies automatically)
cmake ..
```

**What happens during `cmake ..`:**

1. **GoogleTest 1.14.0** is downloaded from:
   - Source: https://github.com/google/googletest.git
   - Tag: v1.14.0
   - Location: `build/_deps/googletest-src/`

2. **pugixml 1.14** is downloaded from:
   - Source: https://github.com/zeux/pugixml.git
   - Tag: v1.14
   - Location: `build/_deps/pugixml-src/`
   - Headers available at: `build/_deps/pugixml-src/src/pugixml.hpp`

3. Both libraries are configured and built automatically
4. Include paths and link libraries are set up automatically

**After first build:** Dependencies are cached in the `build/_deps/` directory. Subsequent builds don't require internet access unless you delete the build directory.

### Quick Start

```bash
# After CMake configuration is complete, build the project
cmake --build .

# Run tests
ctest --verbose

# Run demo application
./src/main               # Linux/macOS
.\src\Debug\main.exe     # Windows (Debug)
.\src\Release\main.exe   # Windows (Release)
```

### Troubleshooting Build Issues

**Problem: CMake can't download dependencies**
```
Solution: Ensure you have internet access during first `cmake ..` run
```

**Problem: Build errors about missing pugixml.hpp**
```
Solution: Delete build directory and re-run cmake:
  rm -rf build
  mkdir build
  cd build
  cmake ..
```

**Problem: Want to update dependencies to latest versions**
```
Solution: Delete _deps folder:
  rm -rf build/_deps
  cd build
  cmake ..
```

### Platform-Specific Instructions

#### Windows (Visual Studio)
```bash
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022"
cmake --build . --config Release
ctest -C Release --verbose
```

#### Linux/macOS
```bash
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)  # or make -j$(sysctl -n hw.ncpu) on macOS
ctest --verbose
```

## Usage

### Option 1: Configuration-Driven (Recommended)

The framework supports XML-based configuration for dynamic task management with hot-reload capabilities.

#### XML Configuration Format

Create a configuration file (e.g., `config/tasks.xml`):

```xml
<?xml version="1.0" encoding="UTF-8"?>
<TaskConfigurations>
    <Task>
        <taskName>TemperatureSensor</taskName>
        <taskType>SensorTask</taskType>
        <intervalMs>1000</intervalMs>
        <sigTolerance>5</sigTolerance>
        <sigRepeat>3</sigRepeat>
        <allowSignal>true</allowSignal>
        <actTolerance>10</actTolerance>
        <actRepeat>0</actRepeat>
        <allowAction>true</allowAction>
    </Task>
    
    <Task>
        <taskName>CoolingFan</taskName>
        <taskType>ActuatorTask</taskType>
        <intervalMs>500</intervalMs>
        <sigTolerance>3</sigTolerance>
        <sigRepeat>0</sigRepeat>
        <allowSignal>true</allowSignal>
        <actTolerance>5</actTolerance>
        <actRepeat>2</actRepeat>
        <allowAction>true</allowAction>
    </Task>
</TaskConfigurations>
```

#### Using ConfigManager

```cpp
#include "core/scheduler.h"
#include "config/config_manager.h"

// Create scheduler
task_scheduler::Scheduler scheduler(4);

// Create ConfigManager with 1-minute debounce window
task_scheduler::ConfigManager configManager(
    scheduler, 
    "config/tasks.xml",
    std::chrono::minutes(1)
);

// Start config-driven management
if (configManager.start()) {
    std::cout << "ConfigManager started successfully\n";
    std::cout << "Watching: config/tasks.xml\n";
    std::cout << "Loaded " << configManager.getTaskCount() << " tasks\n";
}

// Tasks are now running and automatically synchronized with config file
// Modify tasks.xml and changes will be applied after debounce window

// Run your application...
std::this_thread::sleep_for(std::chrono::minutes(5));

// Clean shutdown
configManager.stop();
```

#### ConfigManager Features

**Hot-Reload:**
- Monitors configuration file for changes
- Applies updates automatically after debounce window
- No application restart required

**Debounce Window:**
- Prevents rapid successive updates
- Configurable delay (default: 5 minutes)
- Single atomic update after window expires

**Task Synchronization:**
- **Add**: New tasks in XML are automatically created
- **Update**: Modified task parameters are updated in-place
- **Remove**: Tasks removed from XML are stopped and deleted

**Error Handling:**
- Invalid XML → keeps existing tasks running
- Unknown task types → skipped with warning
- Missing required fields → validation error logged

#### XML Configuration Reference

| Field | Required | Type | Default | Description |
|-------|----------|------|---------|-------------|
| `taskName` | Yes | string | - | Unique task identifier |
| `taskType` | Yes | string | - | "SensorTask" or "ActuatorTask" |
| `intervalMs` | Yes | int | - | Execution interval (milliseconds) |
| `sigTolerance` | No | int | 10 | Signal activation threshold |
| `sigRepeat` | No | int | 0 | Signal heartbeat interval (0=single-shot) |
| `allowSignal` | No | bool | true | Signal channel enable/disable |
| `actTolerance` | No | int | 10 | Action activation threshold |
| `actRepeat` | No | int | 0 | Action heartbeat interval (0=single-shot) |
| `allowAction` | No | bool | true | Action channel enable/disable |

#### Example: Hot-Reload Workflow

```bash
# 1. Start application with ConfigManager
./build/src/main

# 2. Edit config/tasks.xml while app is running
#    - Change intervalMs from 1000 to 500
#    - Add new task
#    - Remove old task

# 3. Save file

# 4. Wait for debounce window (e.g., 1 minute)

# 5. ConfigManager automatically:
#    - Detects file change
#    - Parses updated XML
#    - Synchronizes tasks (add/update/remove)
#    - Logs changes to console

# Application continues running with new configuration!
```

### Option 2: Programmatic Task Creation

For scenarios requiring dynamic task creation or custom task types:

#### Creating Custom Tasks

```cpp
#include "task_base.h"

class MyTask : public task_scheduler::TaskBase {
public:
    MyTask(const std::string& name, int intervalMs)
        : TaskBase(name, intervalMs) {}

    // Report what you want to do
    PlanResult plan() override {
        bool shouldSignal = /* your condition */;
        bool shouldAct = /* your condition */;
        return {shouldSignal, shouldAct};
    }

    // Handle signal activation/deactivation
    void signal(bool doSignal) override {
        if (doSignal) {
            // Activate signal
        } else {
            // Deactivate signal
        }
    }

    // Handle action activation/deactivation
    void act(bool doAct) override {
        if (doAct) {
            // Perform action
        } else {
            // Stop action
        }
    }
};
```

### Using the Scheduler

```cpp
#include "scheduler.h"

// Create scheduler with 4 worker threads
task_scheduler::Scheduler scheduler(4);

// Create task
scheduler.createTask("MyTask", []() {
    return std::make_shared<MyTask>("MyTask", 1000);
});

// Update task configuration (method 1: individual parameters)
scheduler.updateTask("MyTask",
    500,    // intervalMs
    10,     // sigTolerance
    5,      // sigRepeat
    true,   // allowSignal
    10,     // actTolerance
    0,      // actRepeat
    true);  // allowAction

// Update task configuration (method 2: using TaskConfig struct - cleaner!)
TaskConfig config;
config.intervalMs = 500;
config.sigTolerance = 10;
config.sigRepeat = 5;
config.allowSignal = true;
config.actTolerance = 10;
config.actRepeat = 0;
config.allowAction = true;
scheduler.updateTask("MyTask", config);

// Stop task
scheduler.stopTask("MyTask");
```

## State Machine Logic

### Signal/Action Channels

Each task has two independent state machine channels:

1. **Signal Channel** - For state reporting
2. **Action Channel** - For performing actions

### Configuration Parameters

| Parameter | Description |
|-----------|-------------|
| `intervalMs` | Execution interval in milliseconds |
| `sigTolerance` | Number of consecutive "true" plans needed to activate signal |
| `sigRepeat` | Heartbeat interval (0 = single shot) |
| `allowSignal` | Global gate for signal channel |
| `actTolerance` | Number of consecutive "true" plans needed to activate action |
| `actRepeat` | Heartbeat interval (0 = single shot) |
| `allowAction` | Global gate for action channel |

### State Transitions

```
NOISE FILTERING:  plan=true for 1-9 runs → No activation
ACTIVATION:       plan=true for 10 runs → signal(true) or act(true)
HEARTBEAT:        After activation, every N runs → re-fire
DEACTIVATION:     plan=false OR gate closed → signal(false) or act(false)
```

## Running Tests

```bash
# Run all tests
cd build
ctest --verbose

# Run specific test suite
./tests/TaskScheduler_Tests --gtest_filter=StateMachineTest.*

# Run with detailed output
./tests/TaskScheduler_Tests --gtest_verbose
```

### Test Coverage

- **Task Lifecycle** (10 tests) - Creation, destruction, scope persistence
- **State Machine** (11 tests) - Debouncing, activation, heartbeat, gates
- **Config Updates** (7 tests) - Dynamic reconfiguration, thread safety
- **Concurrency** (6 tests) - Race conditions, stress testing
- **Scheduler** (8 tests) - Queue operations, timing accuracy
- **Lazy Deletion** (9 tests) - Task cleanup, memory management

## Demo Application

The demo application demonstrates:

1. **Scope Test** - Tasks persist after creator scope ends
2. **Dynamic Updates** - Frequency changes from 1000ms to 200ms
3. **Safety Gates** - Disabling signal channel at runtime
4. **Lazy Deletion** - Clean task shutdown

```bash
./src/main
```

Expected output shows timestamped task execution with visible frequency changes.

## Architecture Highlights

### Lazy Deletion Pattern
Instead of searching and removing tasks from the priority queue (O(n)), tasks are marked inactive and dropped when naturally popped from queues.

### Snapshotting for Thread Safety
Configuration is locked briefly, copied to local variables, then unlocked before processing - minimizing critical sections.

### Condition Variable Efficiency
Timer and worker threads sleep on condition variables, eliminating busy-waiting for near-zero idle CPU usage.

## Performance

- **Idle CPU**: ~0% (efficient condition variable usage)
- **Latency**: Sub-millisecond task scheduling overhead
- **Throughput**: Supports hundreds of concurrent tasks
- **Memory**: Minimal overhead with smart pointer management

## Contributing

Contributions welcome! Please ensure:
- All tests pass (`ctest`)
- Code follows C++17 best practices
- New features include comprehensive tests

## License

See [LICENSE](LICENSE) file for details.

## Authors

- Rashmi Rout ([@rashmirrout](https://github.com/rashmirrout))

## Acknowledgments

Built with modern C++17 standards and GoogleTest framework.
