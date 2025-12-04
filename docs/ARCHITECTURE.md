# TaskScheduler Framework - Architecture Document

## Table of Contents
1. [System Overview](#system-overview)
2. [Modules](#modules)
3. [State Machine Design](#state-machine-design)
4. [Algorithms & Logic](#algorithms--logic)
5. [Logic Table (Behavior Confirmation)](#logic-table-behavior-confirmation)
6. [Design Choices & Problem Solutions](#design-choices--problem-solutions)
7. [Threading Architecture](#threading-architecture)
8. [Memory Management & Task Lifecycle](#memory-management--task-lifecycle)
9. [Examples & Walkthroughs](#examples--walkthroughs)

---

## System Overview

### High-Level Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                         Scheduler                                │
│  ┌────────────────┐  ┌──────────────┐  ┌──────────────────┐    │
│  │   Registry     │  │ Timer Thread │  │ Worker Thread    │    │
│  │  (shared_ptr)  │  │ Priority Q   │  │ Pool (FIFO)      │    │
│  └────────────────┘  └──────────────┘  └──────────────────┘    │
└─────────────────────────────────────────────────────────────────┘
                              │
                              │ schedules/executes
                              ▼
                    ┌──────────────────┐
                    │    TaskBase      │
                    │  (Template       │
                    │   Method)        │
                    └──────────────────┘
                              │
                              │ calls abstract methods
                              ▼
            ┌─────────────────────────────────┐
            │                                 │
      ┌─────▼──────┐                  ┌──────▼──────┐
      │ SensorTask │                  │ActuatorTask │
      │  (derived) │                  │  (derived)  │
      └────────────┘                  └─────────────┘
```

### Component Interaction Flow

```
1. User creates task → Scheduler::createTask()
2. Task added to Registry (shared_ptr ownership)
3. Task scheduled in Priority Queue (by nextRunTime)
4. Timer Thread wakes, moves task to Worker Queue
5. Worker Thread picks task, calls task->run()
6. TaskBase::run() orchestrates state machine
7. Task rescheduled if still active
8. Lazy Deletion: inactive tasks dropped when popped
```

### Key Design Patterns

- **Template Method Pattern**: `TaskBase::run()` implements algorithm, derived classes provide business logic
- **Registry Pattern**: Central ownership via `shared_ptr` for lifecycle management
- **Lazy Deletion**: O(1) removal by marking inactive, dropping when naturally encountered
- **Thread-Safe Configuration Snapshotting**: Lock-copy-unlock to minimize critical sections

---

## Project Structure

### Directory Organization

The project follows a modular structure with clear separation of concerns:

```
TaskScheduler/
├── include/
│   ├── core/              # Core scheduling engine
│   │   ├── types.h        # Fundamental data structures
│   │   ├── task_base.h    # Abstract task interface & state machine
│   │   └── scheduler.h    # Thread-safe task orchestration
│   ├── tasks/             # Concrete task implementations
│   │   ├── sensor_task.h
│   │   ├── actuator_task.h
│   │   └── task_factory.h
│   └── config/            # Configuration management
│       ├── config_parser.h
│       ├── file_watcher.h
│       └── config_manager.h
├── src/
│   ├── core/              # Core implementation
│   │   ├── task_base.cpp
│   │   └── scheduler.cpp
│   ├── tasks/             # Task implementations
│   │   ├── sensor_task.cpp
│   │   ├── actuator_task.cpp
│   │   └── task_factory.cpp
│   ├── config/            # Configuration implementation
│   │   ├── config_parser.cpp
│   │   ├── file_watcher.cpp
│   │   └── config_manager.cpp
│   └── main.cpp
├── tests/                 # Unit and integration tests
├── config/                # XML configuration files
└── docs/                  # Documentation
```

### Module Responsibilities

**Core Module** (`core/`):
- Contains the "brain and center" of the framework
- `task_base.h/cpp`: Template Method pattern implementation, state machine logic
- `scheduler.h/cpp`: Thread pool, priority queue, task lifecycle management
- `types.h`: Shared data structures (TaskConfig, PlanResult, ScheduleEntry)

**Tasks Module** (`tasks/`):
- Concrete task implementations demonstrating framework usage
- `sensor_task.h/cpp`: Signal-focused task (monitoring, alerts)
- `actuator_task.h/cpp`: Action-focused task (commands, control)
- `task_factory.h/cpp`: Factory pattern for creating tasks from configurations

**Config Module** (`config/`):
- Configuration-driven task management
- `config_parser.h/cpp`: XML parsing and validation
- `file_watcher.h/cpp`: File system monitoring
- `config_manager.h/cpp`: Hot-reload, debouncing, task synchronization

### Design Rationale

1. **Core as Foundation**: `task_base` placed in `core/` because it's the fundamental abstraction that all tasks depend on
2. **Separation of Concerns**: Clear boundaries between scheduling (core), implementations (tasks), and configuration (config)
3. **Include Path Convention**: Use subfolder in includes (e.g., `#include "core/scheduler.h"`) for clarity and namespace organization
4. **Testability**: Modular structure facilitates unit testing of individual components

---

## Modules

### 1. Core Module - Types (`include/core/types.h`)

**Purpose**: Core data structures used across the framework

**Key Types**:

```cpp
struct PlanResult {
    bool wantSignal;  // Signal channel intent
    bool wantAct;     // Action channel intent
};

struct TaskConfig {
    int intervalMs;        // Execution interval
    
    // Signal channel
    int sigTolerance;      // Debounce threshold
    int sigRepeat;         // Heartbeat interval (0 = single-shot)
    bool allowSignal;      // Safety gate
    
    // Action channel (independent)
    int actTolerance;
    int actRepeat;
    bool allowAction;
};

struct ScheduleEntry {
    std::chrono::steady_clock::time_point nextRunTime;
    std::shared_ptr<TaskBase> task;
    
    bool operator>(const ScheduleEntry& other) const;  // For priority queue
};
```

**Design Rationale**:
- Separate struct for configuration enables elegant snapshotting
- Independent channel configurations allow complex behaviors
- ScheduleEntry encapsulates scheduling metadata

---

### 2. TaskBase Module (`include/task_base.h`, `src/task_base.cpp`)

**Purpose**: Abstract base class implementing state machine logic

**Responsibilities**:
- Execute Template Method pattern (`run()` method)
- Manage dual independent state machines (Signal & Action channels)
- Handle configuration updates (thread-safe)
- Provide abstract interface for derived classes

**Key Members**:

```cpp
class TaskBase {
    // Identity & Lifecycle
    std::string name_;
    std::atomic<bool> active_;
    
    // Configuration (mutex-protected)
    mutable std::mutex configMutex_;
    TaskConfig config_;
    
    // State Machine State (single-threaded access via run())
    int sigCounter_;      // Signal channel persistence counter
    bool isSignaled_;     // Signal channel state
    int actCounter_;      // Action channel persistence counter
    bool isActing_;       // Action channel state
};
```

**Public Interface**:
- `void run()`: Template method - orchestrates execution
- `virtual PlanResult plan() = 0`: Derived class provides intent
- `virtual void signal(bool) = 0`: Signal channel state change
- `virtual void act(bool) = 0`: Action channel state change
- `void updateConfig(...)`: Thread-safe configuration update

---

### 3. Scheduler Module (`include/scheduler.h`, `src/scheduler.cpp`)

**Purpose**: Thread-safe task orchestration with priority queue and worker pool

**Architecture**:

```
┌─────────────────────────────────────────────────────┐
│                    Scheduler                         │
│                                                      │
│  Registry (ownership)                               │
│  ┌───────────────────────────────────┐              │
│  │ unordered_map<name, shared_ptr>   │              │
│  └───────────────────────────────────┘              │
│           │                                          │
│           │ reference                                │
│           ▼                                          │
│  Timer Thread              Worker Thread Pool        │
│  ┌─────────────────┐      ┌──────────────────┐     │
│  │ Priority Queue  │──┬──►│ Worker Queue     │     │
│  │ (by time)       │  │   │ (FIFO)           │     │
│  └─────────────────┘  │   └──────────────────┘     │
│                       │            │                │
│                       │            │ execute        │
│                       │            ▼                │
│                       │   ┌──────────────────┐     │
│                       │   │  task->run()     │     │
│                       │   └──────────────────┘     │
│                       │            │                │
│                       │            │ reschedule     │
│                       └────────────┘                │
└─────────────────────────────────────────────────────┘
```

**Key Components**:

1. **Registry**: `unordered_map<string, shared_ptr<TaskBase>>`
   - Keeps tasks alive across scope boundaries
   - Provides O(1) lookup by name
   - Protected by `registryMutex_`

2. **Timer Queue**: `priority_queue<ScheduleEntry>`
   - Orders tasks by `nextRunTime` (earliest = highest priority)
   - Timer thread sleeps until next task ready
   - Protected by `timerMutex_` + `timerCV_`

3. **Worker Queue**: `queue<shared_ptr<TaskBase>>`
   - FIFO execution queue
   - Workers block on empty queue
   - Protected by `workerMutex_` + `workerCV_`

**Thread Model**:
- 1 Timer Thread: manages scheduling timing
- N Worker Threads: execute tasks in parallel
- Condition variables: efficient sleeping (zero busy-wait)

---

### 4. Concrete Task Implementations

**SensorTask** (`include/sensor_task.h`, `src/sensor_task.cpp`):
- Simulates sensor reading
- Implements `plan()`, `signal()`, `act()`
- Demonstrates independent channel usage

**ActuatorTask** (`include/actuator_task.h`, `src/actuator_task.cpp`):
- Simulates actuator control
- Shows action-focused channel usage
- Example of Template Method pattern in practice

---

## State Machine Design

### Dual Independent State Machines

Each task has **TWO independent state machines**:
1. **Signal Channel**: Typically for sensor alerts, notifications
2. **Action Channel**: Typically for actuator commands, control

**Independence**: Channels operate completely independently - different tolerances, repeat intervals, and gates.

### State Machine Structure (Per Channel)

```
                    ┌─────────────────────────────┐
                    │   INACTIVE (Initial State)  │
                    └──────────────┬──────────────┘
                                   │
                  wantX = true     │ counter++
                  counter reaches  │ tolerance
                  gate is open     │
                                   ▼
                    ┌─────────────────────────────┐
            ┌──────►│   ACTIVE (Signaled/Acting)  │◄──────┐
            │       └──────────────┬──────────────┘       │
            │                      │                       │
            │  HEARTBEAT:          │ WITHDRAWAL:           │
            │  repeat > 0,         │ wantX = false OR      │
            │  delta >= repeat     │ gate closed           │
            │  → re-fire           │ → deactivate          │
            │  (snap-back)         │                       │
            └──────────────────────┘                       │
                                                           │
                                                           ▼
                                         ┌─────────────────────────────┐
                                         │   INACTIVE (Reset)          │
                                         └─────────────────────────────┘
```

### State Transition Scenarios

Each channel has **THREE transition scenarios**:

#### Scenario 1: WITHDRAWAL (Falling Edge or Gate Blocked)

```
Condition: (!wantX || !gateOpen) && isXing_
Action:
  1. Call X(false) to deactivate
  2. Set isXing_ = false
  3. Reset counter = 0
```

**Example**:
```
Signal channel active, user intent becomes false
→ signal(false) called
→ isSignaled_ = false
→ sigCounter_ = 0
```

#### Scenario 2: ACTIVATION (Rising Edge)

```
Condition: conditionMet && gateOpen && !isXing_
Action:
  1. Call X(true) to activate
  2. Set isXing_ = true
  (counter stays at tolerance threshold)
```

**Example**:
```
sigCounter reaches sigTolerance (10)
allowSignal gate is open
Not currently signaled
→ signal(true) called
→ isSignaled_ = true
```

#### Scenario 3: REPEAT/HEARTBEAT (Steady State)

```
Condition: conditionMet && gateOpen && isXing_ && (repeat > 0)
Action (if delta >= repeat):
  1. Call X(true) to re-fire
  2. SNAP-BACK: counter = tolerance (reset to baseline)
```

**Example**:
```
sigTolerance = 10, sigRepeat = 5
sigCounter reaches 15 (delta = 5)
→ signal(true) called (heartbeat)
→ sigCounter = 10 (snap-back to baseline)
```

### Visual State Diagram

```
  wantSignal = true (counter++)
          │
          ▼
     ┌────────┐
     │Counter │
     │  < 10  │ (tolerance)
     └────┬───┘
          │
          │ counter reaches 10
          ▼
     ┌─────────────┐
     │ ACTIVATION  │──────┐
     │ (Rising     │      │
     │  Edge)      │      │ isSignaled_ = true
     └─────────────┘      │ signal(true) called
                          │
                          ▼
                   ┌──────────────┐
          ┌───────►│   ACTIVE     │
          │        │  isSignaled_=│
          │        │    true      │
          │        └──────┬───────┘
          │               │
          │               │ counter continues (11, 12, 13...)
          │               │
          │               ▼
          │        ┌──────────────┐
          │        │ delta >= 5?  │ (repeat interval)
          │        └──────┬───────┘
          │               │ YES
          │               ▼
          │        ┌──────────────┐
          │        │  HEARTBEAT   │
          │        │ signal(true) │
          │        │ counter = 10 │ (snap-back)
          │        └──────────────┘
          │               │
          └───────────────┘
                          
     wantSignal = false OR gate closed
          │
          ▼
     ┌─────────────┐
     │ WITHDRAWAL  │
     │ (Falling    │
     │  Edge)      │
     └─────┬───────┘
           │
           │ isSignaled_ = false
           │ signal(false) called
           │ counter = 0
           ▼
      [INACTIVE]
```

---

## Algorithms & Logic

### 1. Configuration Snapshotting Algorithm

**Objective**: Minimize mutex lock time to prevent blocking

**Implementation**:
```cpp
void TaskBase::run() {
    // STEP 1: Configuration Snapshotting
    TaskConfig cfg;
    {
        std::lock_guard<std::mutex> lock(configMutex_);
        cfg = config_;  // Quick struct copy
    }  // Lock released immediately
    
    // STEP 2-4: Process with local copy (no locks)
    PlanResult intent = plan();
    processSignalChannel(cfg, intent.wantSignal);
    processActionChannel(cfg, intent.wantAct);
}
```

**Benefits**:
- Lock held only for struct copy (~50 nanoseconds)
- Processing happens with local copy (no contention)
- Configuration updates don't block task execution
- Ensures consistent configuration snapshot for entire run cycle

---

### 2. Debounce/Tolerance Algorithm

**Purpose**: Filter noise and prevent "chatter"

**Persistence Counter Logic**:

```cpp
// Counter Management
if (wantSignal) {
    sigCounter_++;  // Accumulate evidence
} else {
    sigCounter_ = 0;  // Immediate reset on first false
}

// Condition Evaluation
bool conditionMet = (sigCounter_ >= cfg.sigTolerance);
```

**Behavior**:
- `wantSignal = true` → counter increments
- `wantSignal = false` → **instant reset to 0**
- Transition requires N **consecutive** true values
- Single false immediately rejects accumulated evidence

**Example Trace**:
```
Tolerance = 3

Cycle  wantSignal  counter  conditionMet
  1       true        1         false
  2       true        2         false
  3       true        3         true   ← ACTIVATION
  4       true        4         true
  5       false       0         false  ← INSTANT RESET
  6       true        1         false
```

---

### 3. Heartbeat/Repeat Interval Algorithm

**Purpose**: Periodic re-firing during steady-state activation

**Delta Calculation**:

```cpp
// Already activated (isSignaled_ = true)
// Counter continues incrementing...

if (cfg.sigRepeat > 0) {
    int delta = sigCounter_ - cfg.sigTolerance;
    
    if (delta >= cfg.sigRepeat) {
        signal(true);                    // Re-fire heartbeat
        sigCounter_ = cfg.sigTolerance;  // SNAP-BACK to baseline
    }
}
```

**Example**:
```
tolerance = 10, repeat = 5

Counter:  10  11  12  13  14  15
Delta:     0   1   2   3   4   5   ← delta >= repeat
Action:   ACTIVATE                  HEARTBEAT (snap-back to 10)
```

**Snap-Back Mechanism**:
- Prevents counter overflow
- Maintains consistent timing
- Delta always measured from tolerance threshold

---

### 4. Priority Queue Scheduling

**Timer Thread Algorithm**:

```cpp
void Scheduler::timerThreadFunc() {
    while (running_) {
        std::unique_lock<std::mutex> lock(timerMutex_);
        
        if (timerQueue_.empty()) {
            timerCV_.wait(lock);  // Sleep until task added
            continue;
        }
        
        auto entry = timerQueue_.top();
        auto now = std::chrono::steady_clock::now();
        
        if (entry.nextRunTime <= now) {
            // Time to execute
            timerQueue_.pop();
            
            if (entry.task->isActive()) {  // Lazy deletion check
                moveToWorkerQueue(entry.task);
            }
        } else {
            // Sleep until next task
            timerCV_.wait_until(lock, entry.nextRunTime);
        }
    }
}
```

**Key Points**:
- Min-heap: earliest time = highest priority
- Condition variable: efficient sleeping
- Lazy deletion: inactive tasks skipped when popped

---

### 5. Worker Thread Pool Algorithm

**Worker Thread Logic**:

```cpp
void Scheduler::workerThreadFunc() {
    while (running_) {
        std::shared_ptr<TaskBase> task;
        
        {
            std::unique_lock<std::mutex> lock(workerMutex_);
            workerCV_.wait(lock, [this] { 
                return !workerQueue_.empty() || !running_; 
            });
            
            if (!running_) break;
            
            task = workerQueue_.front();
            workerQueue_.pop();
        }
        
        if (task && task->isActive()) {  // Lazy deletion check
            task->run();                 // Execute state machine
            rescheduleTask(task);        // Put back in timer queue
        }
    }
}
```

**Benefits**:
- FIFO fairness
- Parallel execution across workers
- Automatic rescheduling
- Lazy deletion at execution point

---

## Logic Table (Behavior Confirmation)

### Signal Channel Truth Table

Variables:
- **W**: `wantSignal` (user intent from plan())
- **C**: `conditionMet` (counter >= tolerance)
- **G**: `gateOpen` (allowSignal safety gate)
- **S**: `isSignaled_` (current state)

Action Codes:
- **NOP**: No operation (maintain current state)
- **ACT**: Call signal(true), set isSignaled_ = true
- **WDR**: Call signal(false), set isSignaled_ = false, reset counter
- **HB**: Heartbeat (if repeat configured and delta >= repeat)

| W | C | G | S | Scenario | Action | Next S | Counter |
|---|---|---|---|----------|--------|--------|---------|
| 0 | 0 | 0 | 0 | No intent, inactive | NOP | 0 | 0 |
| 0 | 0 | 0 | 1 | No intent, was active | WDR | 0 | 0 |
| 0 | 0 | 1 | 0 | No intent, gate open | NOP | 0 | 0 |
| 0 | 0 | 1 | 1 | No intent, was active | WDR | 0 | 0 |
| 0 | 1 | 0 | 0 | Impossible* | - | - | - |
| 0 | 1 | 0 | 1 | Impossible* | - | - | - |
| 0 | 1 | 1 | 0 | Impossible* | - | - | - |
| 0 | 1 | 1 | 1 | Impossible* | - | - | - |
| 1 | 0 | 0 | 0 | Intent but not ready, gate closed | NOP | 0 | cnt++ |
| 1 | 0 | 0 | 1 | Intent but gate closed | WDR | 0 | 0 |
| 1 | 0 | 1 | 0 | Intent but not ready | NOP | 0 | cnt++ |
| 1 | 0 | 1 | 1 | Intent but dropped below tolerance | WDR | 0 | 0 |
| 1 | 1 | 0 | 0 | Ready but gate closed | NOP | 0 | cnt++ |
| 1 | 1 | 0 | 1 | Active but gate closed | WDR | 0 | 0 |
| 1 | 1 | 1 | 0 | Ready & gate open | ACT | 1 | same |
| 1 | 1 | 1 | 1 | Steady state active | HB** | 1 | snap-back |

\* Impossible because W=0 resets counter to 0, so C cannot be 1

\** HB only fires if repeat > 0 and delta >= repeat, otherwise NOP

### Action Channel Truth Table

Identical structure, replace:
- `wantSignal` → `wantAct`
- `allowSignal` → `allowAction`
- `isSignaled_` → `isActing_`
- `signal()` → `act()`

### State Coverage

The state machine handles **all 16 possible combinations** for each channel:
- 2^4 = 16 states (W, C, G, S)
- 8 states impossible (W=0 forces C=0)
- **8 valid states** fully covered by 3 scenarios

---

## Design Choices & Problem Solutions

### 1. "Chatter" or "Flooding" Problem Solution

**Problem**:
```
Noisy sensor oscillating rapidly:
true, false, true, false, true, false...

Without debouncing:
signal(true), signal(false), signal(true), signal(false)...
→ Floods downstream systems with spurious events
→ Wastes resources on transient glitches
```

**Solution: Persistence Counter with Tolerance**

```cpp
// Requires N consecutive "true" values
if (wantSignal) {
    sigCounter_++;  // Build evidence
} else {
    sigCounter_ = 0;  // Instant rejection of noise
}

bool conditionMet = (sigCounter_ >= cfg.sigTolerance);
```

**Behavior**:

```
Noisy input with tolerance = 3:

Cycle:     1  2  3  4  5  6  7  8  9  10
Input:     T  F  T  T  T  F  T  T  T  T
Counter:   1  0  1  2  3  0  1  2  3  4
Activated: -  -  -  -  - NO  -  -  - YES

First sequence rejected (interrupted by false at cycle 2)
Second sequence rejected (interrupted by false at cycle 6)
Third sequence succeeds (3 consecutive trues: cycles 8-10)
```

**Benefits**:
- Filters transient spikes and glitches
- Requires sustained signal to activate
- **Instant reset** on first false prevents partial accumulation
- Configurable tolerance adapts to noise characteristics

**Hysteresis Effect**:
- Harder to turn ON (must accumulate N trues)
- Easy to turn OFF (single false)
- Prevents rapid oscillation at threshold

---

### 2. State vs. Configuration Dynamics

**Design Principle**: Separation of concerns for thread safety and performance

#### Configuration (Mutex-Protected)

**Characteristics**:
- Changes infrequently (user-driven updates)
- Needs thread-safe updates from external threads
- Read-frequently by run() method

**Members**:
```cpp
// Protected by configMutex_
TaskConfig config_;  // All 7 configuration parameters
```

**Access Pattern**:
```cpp
// Write (infrequent):
void updateConfig(const TaskConfig& config) {
    std::lock_guard<std::mutex> lock(configMutex_);
    config_ = config;  // Quick write
}

// Read (every cycle):
TaskConfig cfg;
{
    std::lock_guard<std::mutex> lock(configMutex_);
    cfg = config_;  // Quick snapshot
}  // Lock released immediately
// Use local copy for entire cycle
```

#### Runtime State (Unlocked)

**Characteristics**:
- Changes every execution cycle
- Only accessed by run() method (single-threaded)
- High-frequency updates (every ~10-100ms)

**Members**:
```cpp
// NO mutex protection (single-threaded access)
int sigCounter_;      // Updated every cycle
bool isSignaled_;     // Updated on transitions
int actCounter_;
bool isActing_;
```

**Why No Mutex?**
```
Only TaskBase::run() modifies these variables
run() is called by single worker thread at a time
Scheduler ensures no concurrent run() calls on same task
→ No data race possible
→ No synchronization overhead needed
```

#### Performance Impact

**With Separation**:
```
Configuration read: 1 lock/unlock per cycle (~50ns)
State updates: 0 locks, direct memory access (~5ns each)
Total overhead per cycle: ~50ns
```

**Without Separation** (hypothetical):
```
Every counter increment: lock/unlock (~50ns)
Every state check: lock/unlock (~50ns)
If 10 state operations per cycle: 500ns overhead
→ 10x slower!
```

#### Thread Safety Guarantee

```
Configuration Thread         │  Worker Thread (run())
                            │
updateConfig() {            │  run() {
  lock(configMutex_);       │    // Snapshot config
  config_ = newConfig;      │    {
  unlock();                 │      lock(configMutex_);
}                           │      cfg = config_;  ← Reads consistent snapshot
                            │      unlock();
                            │    }
                            │    
                            │    // Process with local copy
                            │    sigCounter_++;  ← No lock needed
                            │    if (conditionMet) ...
                            │  }
```

**Benefits**:
1. **Performance**: Minimal locking overhead
2. **Consistency**: Entire cycle uses same configuration snapshot
3. **Safety**: Configuration updates atomic and thread-safe
4. **Clarity**: Clear separation of concerns

---

### 3. "Persistence Counter" or "Debounce Logic"

**Concept**: Accumulate evidence over time before acting

#### Counter Behavior

**Increment on True**:
```cpp
if (wantSignal) {
    sigCounter_++;  // Accumulate positive evidence
}
```

**Instant Reset on False**:
```cpp
else {
    sigCounter_ = 0;  // Reject any partial evidence
}
```

**Why Instant Reset?**
```
Alternative: Decrement on false
  Problem: Allows "credit" for past trues
  
Example with decrement:
  T T T F T T → counter: 1,2,3,2,3,4 → ACTIVATES
  
Actual with reset:
  T T T F T T → counter: 1,2,3,0,1,2 → REJECTED
  
Instant reset prevents "memory" of interrupted sequences
→ Requires truly consecutive trues
→ Stronger noise immunity
```

#### Threshold Evaluation

```cpp
bool conditionMet = (sigCounter_ >= cfg.sigTolerance);

// Must maintain >= tolerance for activation
// Dropping below tolerance causes withdrawal
```

#### Example Scenario

**Configuration**:
- `sigTolerance = 5`
- `sigRepeat = 3`

**Execution Trace**:

```
Cycle  wantSignal  counter  conditionMet  isSignaled_  Action
  1       false       0         false        false      -
  2       true        1         false        false      -
  3       true        2         false        false      -
  4       true        3         false        false      -
  5       true        4         false        false      -
  6       true        5         true         false      ACTIVATE → signal(true)
  7       true        6         true         true       (steady state)
  8       true        7         true         true       (steady state)
  9       true        8         true         true       HEARTBEAT → signal(true), snap to 5
 10       true        6         true         true       (after snap-back)
 11       true        7         true         true       -
 12       true        8         true         true       HEARTBEAT → signal(true), snap to 5
 13       false       0         false        true       WITHDRAW → signal(false)
 14       false       0         false        false      -
```

**Key Observations**:
- Activation delayed until cycle 6 (5 consecutive trues accumulated)
- Heartbeat fires every 3 cycles after activation (cycles 9, 12)
- Snap-back keeps counter at baseline (5) after each heartbeat
- Single false (cycle 13) instantly deactivates and resets counter

#### Debounce Timing

**Minimum Activation Time**:
```
Time = tolerance × intervalMs

Example:
  tolerance = 10
  intervalMs = 100ms
  Minimum = 10 × 100ms = 1000ms (1 second)
```

**Real-World Application**:
```
Button press detection:
  tolerance = 3, intervalMs = 50ms
  → Must hold button for 150ms minimum
  → Filters mechanical bounce (typically 5-10ms)
  
Sensor reading:
  tolerance = 10, intervalMs = 100ms
  → Must detect condition for 1 second
  → Filters environmental noise and transients
```

---

### 4. Shared State Modification

**Problem**: How to safely modify state across multiple threads?

**Solution**: Architectural constraint + careful locking

#### Architectural Guarantee

```
RULE: Only TaskBase::run() modifies runtime state
      (sigCounter_, isSignaled_, actCounter_, isActing_)

Enforcement:
  1. run() is non-virtual (cannot be overridden)
  2. State variables are private
  3. No public setters for state
  4. Derived classes cannot access state directly
```

#### Thread Interaction Model

```
┌─────────────────────────────────────────────────────┐
│  External Thread (User/Scheduler)                   │
│                                                      │
│  Can only call:                                      │
│    - updateConfig() [with mutex]                    │
│    - setActive()    [atomic flag]                   │
│    - getName()      [const, immutable]              │
│                                                      │
│  CANNOT modify state variables                      │
└─────────────────────────────────────────────────────┘
                        │
                        │ schedules
                        ▼
┌─────────────────────────────────────────────────────┐
│  Worker Thread (Scheduler-controlled)               │
│                                                      │
│  Calls: task->run()                                 │
│                                                      │
│  run() {                                            │
│    // Snapshot config (with mutex)                  │
│    TaskConfig cfg;                                  │
│    { lock; cfg = config_; unlock; }                 │
│                                                      │
│    // Process state (NO mutex)                      │
│    sigCounter_++;  ← EXCLUSIVE ACCESS               │
│    if (conditionMet) {                              │
│      isSignaled_ = true;  ← EXCLUSIVE ACCESS        │
│      signal(true);                                   │
│    }                                                │
│  }                                                  │
└─────────────────────────────────────────────────────┘
```

#### Why No Mutex for State?

**Scheduler Guarantee**:
```cpp
// Scheduler ensures no concurrent execution
void Scheduler::workerThreadFunc() {
    while (running_) {
        auto task = getNextTask();
        
        // Only ONE worker calls run() at a time per task
        task->run();  
        
        // Task won't be in queue again until rescheduled
        rescheduleTask(task);
    }
}
```

**Proof of Safety**:
```
1. Task removed from worker queue
2. run() executes (exclusive access to state)
3. run() completes
4. Task rescheduled to timer queue
5. Timer queue eventually moves back to worker queue
6. Process repeats

At no point can two threads call run() concurrently on same task
→ State access is inherently serialized
→ No mutex needed
```

#### Configuration Updates During run()

**Scenario**: User calls updateConfig() while run() is executing

```
Thread 1 (Worker)         │  Thread 2 (User/Config)
                          │
run() {                   │  
  {                       │  
    lock(configMutex_);   │  updateConfig(newConfig) {
    cfg = config_;        │    // Waits for lock...
    unlock();             │
  }                       │    lock(configMutex_);  ← Acquires
                          │    config_ = newConfig;
  // Processing with      │    unlock();
  // old snapshot         │  }
  sigCounter_++;          │
  processChannels(cfg);   │
}                         │
                          │
// Next cycle            │
run() {                   │
  {                       │
    lock(configMutex_);   │
    cfg = config_;        │  ← Sees new config
    unlock();             │
  }                       │
  // Uses new config      │
}                         │
```

**Result**:
- Configuration update is atomic
- Current cycle completes with old config (consistency)
- Next cycle uses new config
- No partial updates or torn reads

#### Comparison with Alternative Approaches

**Approach 1: Mutex on Every State Access** (Rejected)
```cpp
// EVERY state modification needs lock
void processSignalChannel(...) {
    lock(stateMutex_);
    sigCounter_++;
    unlock();
    
    lock(stateMutex_);
    if (sigCounter_ >= tolerance) {
        unlock();
        lock(stateMutex_);
        isSignaled_ = true;
        unlock();
    }
}
```
- **Problem**: 10x overhead, lock contention, complexity

**Approach 2: Atomic Variables** (Rejected)
```cpp
std::atomic<int> sigCounter_;
std::atomic<bool> isSignaled_;
```
- **Problem**: Cannot atomically update multiple related variables
- **Problem**: Read-modify-write sequences not atomic across variables

**Chosen Approach: Architectural Constraint** (Selected)
```cpp
// run() is the ONLY modifier
// No locks needed because no concurrent access
void run() {
    sigCounter_++;  // Direct access, no overhead
    isSignaled_ = true;  // Direct access
}
```
- **Benefit**: Zero overhead
- **Benefit**: Simple, maintainable code
- **Benefit**: Guaranteed safety through design

---

### 5. Memory Management & Task Lifecycle

**Problem**: How to manage task lifetime across async operations?

**Solution**: Registry pattern + shared_ptr + lazy deletion

#### Task Ownership Model

```
┌──────────────────────────────────────────────────┐
│            Registry (Master Owner)                │
│  unordered_map<string, shared_ptr<TaskBase>>     │
│                                                   │
│  Keeps tasks alive until explicitly removed      │
└───────────────┬──────────────────────────────────┘
                │ shared_ptr references
                ▼
        ┌───────────────┐
        │  Timer Queue  │ (weak ownership via shared_ptr copy)
        └───────┬───────┘
                │
                ▼
        ┌───────────────┐
        │ Worker Queue  │ (weak ownership via shared_ptr copy)
        └───────┬───────┘
                │
                ▼
        ┌───────────────┐
        │  Worker Thread│ (temporary shared_ptr during run())
        └───────────────┘
```

**Reference Counting**:
```cpp
// Task created
auto task = std::make_shared<SensorTask>(...);

// Registry holds it (ref count = 1)
registry_["sensor1"] = task;

// Scheduled to timer queue (ref count = 2)
timerQueue_.push(ScheduleEntry{nextTime, task});

// Timer pops, moves to worker queue (ref count = 2)
workerQueue_.push(task);

// Worker picks up (ref count = 2)
auto taskCopy = workerQueue_.front();

// Worker executes
taskCopy->run();

// Worker reschedules (ref count = 2)
timerQueue_.push(ScheduleEntry{nextTime, taskCopy});

// Worker's copy destroyed (ref count = 1)
// Only registry holds reference
```

#### Lazy Deletion Algorithm

**Problem**: Removing from priority_queue is O(n)

**Solution**: Mark inactive, drop when naturally encountered

```cpp
// User requests task stop
bool Scheduler::stopTask(const std::string& name) {
    std::lock_guard<std::mutex> lock(registryMutex_);
    
    auto it = registry_.find(name);
    if (it == registry_.end()) return false;
    
    // Mark inactive (O(1))
    it->second->setActive(false);
    
    // Remove from registry (O(1))
    registry_.erase(it);
    
    // Task still in queues, but will be dropped when popped
    return true;
}

// Timer thread checks on pop
void Scheduler::timerThreadFunc() {
    auto entry = timerQueue_.top();
    timerQueue_.pop();
    
    if (entry.task->isActive()) {  // Lazy deletion check
        moveToWorkerQueue(entry.task);
    }
    // else: drop silently, task will be destroyed when ref count = 0
}

// Worker thread checks before execution
void Scheduler::workerThreadFunc() {
    auto task = workerQueue_.front();
    workerQueue_.pop();
    
    if (task->isActive()) {  // Lazy deletion check
        task->run();
        rescheduleTask(task);
    }
    // else: drop silently
}
```

**Benefits**:
- **O(1) removal** (vs O(n) to search/remove from priority_queue)
- **Automatic cleanup**: ref count drops to 0, destructor called
- **No memory leaks**: RAII handles everything
- **Safe across threads**: inactive flag is atomic

#### Task Lifecycle Diagram

```
CREATE
  │
  └──► Registry.insert(name, shared_ptr)  [ref = 1]
         │
         └──► scheduleTask()
                │
                └──► timerQueue.push(entry)  [ref = 2]
                       │
                       ▼
        ┌──────────────────────────────┐
        │   SCHEDULED (in timer queue) │
        └──────────────┬───────────────┘
                       │
                       │ time reached
                       ▼
                timerQueue.pop()
                       │
                       ├──► if (!active) → DROP  [ref = 1]
                       │
                       └──► workerQueue.push()  [ref = 2]
                              │
                              ▼
        ┌──────────────────────────────┐
        │   QUEUED (in worker queue)   │
        └──────────────┬───────────────┘
                       │
                       │ worker available
                       ▼
                workerQueue.pop()
                       │
                       ├──► if (!active) → DROP  [ref = 1]
                       │
                       └──► task->run()
                              │
                              ▼
        ┌──────────────────────────────┐
        │   EXECUTING (run() active)   │
        └──────────────┬───────────────┘
                       │
                       ├──► rescheduleTask()  [ref = 2]
                       │      │
                       │      └──► Back to SCHEDULED
                       │
                       └──► (if stopped, ref drops)
                              │
                              ▼
        ┌──────────────────────────────┐
        │   STOPPED (inactive = true)  │
        │   Registry removed [ref = 1] │
        └──────────────┬───────────────┘
                       │
                       │ all queue refs cleared
                       ▼
                   [ref = 0]
                       │
                       └──► DESTROYED (destructor called)
```

#### Shutdown Sequence

```cpp
Scheduler::~Scheduler() {
    shutdown();
}

void Scheduler::shutdown() {
    // 1. Signal all threads to stop
    running_.store(false);
    
    // 2. Wake sleeping threads
    timerCV_.notify_all();
    workerCV_.notify_all();
    
    // 3. Join all threads
    if (timerThread_.joinable()) {
        timerThread_.join();
    }
    for (auto& worker : workerThreads_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    
    // 4. Clear registry
    {
        std::lock_guard<std::mutex> lock(registryMutex_);
        registry_.clear();  // All shared_ptrs released
    }
    
    // 5. Queues destroyed (local to timer/worker threads)
    // → All remaining shared_ptrs released
    // → All tasks destroyed automatically
}
```

**Reference Count Trace During Shutdown**:
```
Before shutdown:
  Registry holds task [ref = 1]
  Timer queue holds task [ref = 2]
  
shutdown() called:
  Threads stop → queues destroyed [ref = 1]
  registry_.clear() [ref = 0]
  
Task destructor called automatically
  → Resources released
  → No manual delete needed
```

#### Memory Safety Guarantees

1. **No Dangling Pointers**: shared_ptr keeps object alive while references exist
2. **No Memory Leaks**: When last shared_ptr destroyed, object deleted
3. **No Double-Free**: shared_ptr handles deletion atomically
4. **Thread-Safe**: shared_ptr ref counting is atomic
5. **Exception-Safe**: RAII ensures cleanup even if exceptions thrown

#### Scope Independence Example

```cpp
// Function scope
void setupTasks() {
    auto sensor = std::make_shared<SensorTask>(...);
    
    scheduler.createTask("sensor1", [sensor]() { return sensor; });
    
    // sensor goes out of scope here
    // BUT: Registry still holds shared_ptr
    // Task continues to exist and execute
}

// Later, from different scope
void stopTasks() {
    scheduler.stopTask("sensor1");
    
    // Task marked inactive
    // Will be dropped from queues
    // When last reference cleared, destructor called
    // No explicit delete needed
}
```

**Result**: Tasks persist across scope boundaries, managed automatically by shared_ptr ownership.

---

## Threading Architecture

### Thread Organization

```
┌────────────────────────────────────────────────────────┐
│                    Main Thread                          │
│  - Creates Scheduler                                    │
│  - Creates Tasks (createTask())                         │
│  - Updates Configs (updateConfig())                     │
│  - Eventually calls shutdown()                          │
└────────────────────────────────────────────────────────┘
                          │
                          │ spawns
                          ▼
┌────────────────────────────────────────────────────────┐
│                  Timer Thread                           │
│  - Manages priority queue                               │
│  - Sleeps until next task ready                         │
│  - Moves ready tasks to worker queue                    │
│  - Single thread (no parallelism needed)                │
└────────────────────────────────────────────────────────┘
                          │
                          │ feeds
                          ▼
┌────────────────────────────────────────────────────────┐
│               Worker Thread Pool                        │
│  - N parallel worker threads                            │
│  - Each worker picks tasks from queue (FIFO)            │
│  - Executes task->run()                                 │
│  - Reschedules task back to timer queue                 │
└────────────────────────────────────────────────────────┘
```

### Synchronization Primitives

#### 1. Mutexes

**Registry Mutex**:
```cpp
std::mutex registryMutex_;

// Protects: unordered_map<string, shared_ptr<TaskBase>>
// Used by: createTask(), stopTask(), updateTask(), getTask()
// Lock duration: Very short (map operations)
```

**Timer Mutex**:
```cpp
std::mutex timerMutex_;

// Protects: priority_queue<ScheduleEntry>
// Used by: Timer thread, scheduleTask(), rescheduleTask()
// Lock duration: Short (queue push/pop)
```

**Worker Mutex**:
```cpp
std::mutex workerMutex_;

// Protects: queue<shared_ptr<TaskBase>>
// Used by: Timer thread (producer), Worker threads (consumers)
// Lock duration: Short (queue push/pop)
```

**Config Mutex** (per task):
```cpp
std::mutex configMutex_;  // in TaskBase

// Protects: TaskConfig config_
// Used by: run() (reader), updateConfig() (writer)
// Lock duration: Very short (struct copy)
```

#### 2. Condition Variables

**Timer CV**:
```cpp
std::condition_variable timerCV_;

// Purpose: Efficient sleeping until next task or new task added
// Notify: When task scheduled (createTask, rescheduleTask)
// Wait: Timer thread sleeps until notified or timeout
```

**Worker CV**:
```cpp
std::condition_variable workerCV_;

// Purpose: Wake workers when tasks available
// Notify: When task moved to worker queue
// Wait: Worker threads sleep when queue empty
```

#### 3. Atomic Flags

**Running Flag**:
```cpp
std::atomic<bool> running_;

// Purpose: Signal shutdown to all threads
// Lock-free: Read without mutex
// Used: All threads check this flag
```

**Active Flag** (per task):
```cpp
std::atomic<bool> active_;  // in TaskBase

// Purpose: Lazy deletion marker
// Lock-free: Read without mutex during pop operations
```

### Deadlock Prevention

**Lock Ordering**: Always acquire in same order
```
1. registryMutex_
2. timerMutex_ OR workerMutex_ (never both simultaneously)
3. configMutex_ (short duration, released quickly)
```

**No Nested Locks**:
```cpp
// GOOD: Single lock scope
{
    std::lock_guard<std::mutex> lock(timerMutex_);
    timerQueue_.push(entry);
}  // Released

// NEVER: Nested locks (could deadlock)
{
    std::lock_guard<std::mutex> lock1(timerMutex_);
    {
        std::lock_guard<std::mutex> lock2(workerMutex_);  // WRONG!
    }
}
```

### Lock-Free Patterns

**Active Flag Check**:
```cpp
// No lock needed (atomic read)
if (!task->isActive()) {
    // Drop task
}
```

**Running Flag Check**:
```cpp
// No lock needed (atomic read)
while (running_.load()) {
    // Thread loop
}
```

### Performance Characteristics

**Timer Thread Wake-up**:
```
Average case: Sleep until exact time (microsecond precision)
Worst case: Spurious wakeup, check condition, sleep again
Overhead: Minimal (condition variable is efficient)
```

**Worker Thread Wake-up**:
```
Empty queue: Workers sleep (zero CPU usage)
Task added: One worker woken (notify_one)
Multiple tasks: Multiple workers wake as queue refills
```

**Lock Contention Analysis**:
```
Registry: Low (infrequent creates/stops)
Timer Queue: Low (single producer/consumer)
Worker Queue: Medium (N consumers)
Config: Very Low (snapshot is quick)
```

---

## Memory Management & Task Lifecycle

### Memory Ownership Strategy

**Registry Pattern**:
```cpp
// Central registry owns all tasks
std::unordered_map<std::string, std::shared_ptr<TaskBase>> registry_;

// Benefits:
// 1. Tasks persist across scope boundaries
// 2. Single source of truth for task existence
// 3. Prevents premature destruction
// 4. Enables lookup by name
```

### Lifecycle States

```
1. CONSTRUCTION
   ↓
2. REGISTRATION (ref count = 1)
   ↓
3. SCHEDULED (ref count = 2)
   ↓
4. EXECUTING (ref count = 2-3)
   ↓
5. RESCHEDULED (ref count = 2)
   ↓ (cycle repeats)
   
6. STOPPED (inactive = true, registry removed, ref count = 1)
   ↓
7. DROPPED (ref count = 0)
   ↓
8. DESTRUCTION (destructor called)
```

### Resource Cleanup

**Automatic via RAII**:
```cpp
class TaskBase {
public:
    virtual ~TaskBase() = default;
    
    // Derived classes can override for cleanup
    // Base class has no resources to clean up
    // Mutexes destroyed automatically
};
```

**Scheduler Cleanup**:
```cpp
Scheduler::~Scheduler() {
    shutdown();  // Stop all threads
    
    // registry_ destroyed → all shared_ptrs released
    // timerQueue_ destroyed → all entries released
    // workerQueue_ destroyed → all entries released
    // Threads joined → safe to destroy
}
```

### Memory Leak Prevention

**Guarantee**: No leaks if following rules:
1. All tasks created via `createTask()` (registry ownership)
2. All tasks stopped via `stopTask()` or `shutdown()`
3. No circular shared_ptr references
4. Scheduler destroyed properly (calls shutdown())

**Validation**:
```cpp
// Test with memory sanitizers
// Valgrind: 0 bytes leaked
// AddressSanitizer: Clean exit
```

---

## Examples & Walkthroughs

### Example 1: Simple Activation Cycle

**Scenario**: Sensor task activates signal after 3 consecutive true readings

**Configuration**:
```cpp
intervalMs = 100
sigTolerance = 3
sigRepeat = 0 (single-shot)
allowSignal = true
```

**Execution Trace**:

| Cycle | Time (ms) | plan() returns | Counter | conditionMet | isSignaled_ | Action |
|-------|-----------|----------------|---------|--------------|-------------|---------|
| 1 | 0 | {false, -} | 0 | false | false | - |
| 2 | 100 | {false, -} | 0 | false | false | - |
| 3 | 200 | {true, -} | 1 | false | false | - |
| 4 | 300 | {true, -} | 2 | false | false | - |
| 5 | 400 | {true, -} | 3 | **true** | false | **signal(true)** |
| 6 | 500 | {true, -} | 4 | true | **true** | - (steady state) |
| 7 | 600 | {false, -} | 0 | false | true | **signal(false)** |

**Key Points**:
- Cycles 1-2: Sensor reads false, counter stays at 0
- Cycle 3: First true, counter = 1
- Cycle 4: Second consecutive true, counter = 2
- Cycle 5: Third consecutive true, **ACTIVATION** (counter reaches tolerance)
- Cycle 6: Continues true, steady state (no heartbeat because repeat = 0)
- Cycle 7: Drops to false, **WITHDRAWAL** (instant deactivation and reset)

---

### Example 2: Noise Rejection

**Scenario**: Noisy sensor with intermittent false readings

**Configuration**:
```cpp
sigTolerance = 5
```

**Execution Trace**:

| Cycle | plan() | Counter | Action | Explanation |
|-------|--------|---------|--------|-------------|
| 1 | true | 1 | - | Start accumulating |
| 2 | true | 2 | - | |
| 3 | **false** | **0** | - | **Noise! Reset counter** |
| 4 | true | 1 | - | Start over |
| 5 | true | 2 | - | |
| 6 | true | 3 | - | |
| 7 | **false** | **0** | - | **Noise again! Reset** |
| 8 | true | 1 | - | Start over again |
| 9 | true | 2 | - | |
| 10 | true | 3 | - | |
| 11 | true | 4 | - | |
| 12 | true | 5 | **signal(true)** | **FINALLY activated** |

**Result**: First two attempts rejected due to intermittent false readings. Activation only occurs after 5 truly consecutive trues.

---

### Example 3: Heartbeat/Repeat Mechanism

**Scenario**: Activated signal with periodic heartbeat

**Configuration**:
```cpp
sigTolerance = 10
sigRepeat = 5
```

**Execution Trace**:

| Cycle | Counter | Delta | Action | Counter After | Explanation |
|-------|---------|-------|--------|---------------|-------------|
| 1-9 | 1-9 | - | - | - | Accumulating to tolerance |
| 10 | 10 | 0 | **signal(true)** | 10 | **ACTIVATION** |
| 11 | 11 | 1 | - | 11 | Steady state |
| 12 | 12 | 2 | - | 12 | |
| 13 | 13 | 3 | - | 13 | |
| 14 | 14 | 4 | - | 14 | |
| 15 | 15 | 5 | **signal(true)** | **10** | **HEARTBEAT + snap-back** |
| 16 | 11 | 1 | - | 11 | After snap-back |
| 17 | 12 | 2 | - | 12 | |
| 18 | 13 | 3 | - | 13 | |
| 19 | 14 | 4 | - | 14 | |
| 20 | 15 | 5 | **signal(true)** | **10** | **HEARTBEAT + snap-back** |

**Key Points**:
- Delta = counter - tolerance
- Heartbeat fires when delta >= repeat (5)
- Snap-back resets counter to tolerance (10)
- Consistent 5-cycle heartbeat period

---

### Example 4: Configuration Update During Execution

**Scenario**: User updates tolerance while task is running

**Timeline**:

```
Thread 1 (Worker)              │  Thread 2 (User)
                               │
Cycle N:                       │
run() {                        │
  {                            │
    lock(configMutex_);        │
    cfg = config_;             │  // cfg.sigTolerance = 10
    unlock();                  │
  }                            │
                               │
  sigCounter_++;               │  // = 9
  conditionMet = (9 >= 10);    │  // false
  // No activation             │
}                              │
                               │  updateConfig() {
                               │    lock(configMutex_);
                               │    config_.sigTolerance = 5;
                               │    unlock();
                               │  }
                               │
Cycle N+1:                     │
run() {                        │
  {                            │
    lock(configMutex_);        │
    cfg = config_;             │  // cfg.sigTolerance = 5 (NEW!)
    unlock();                  │
  }                            │
                               │
  // counter still at 9        │
  conditionMet = (9 >= 5);     │  // TRUE!
  signal(true);                │  // ACTIVATION
}                              │
```

**Result**: Configuration change takes effect immediately in next cycle, causing activation with accumulated counter.

---

### Example 5: Lazy Deletion Walkthrough

**Scenario**: User stops task while it's scheduled

**Timeline**:

```
T0: Task created and scheduled
    Registry: {task1: shared_ptr [ref=1]}
    Timer Queue: [{time=T5, task1}] [ref=2]

T1: User calls stopTask("task1")
    task1->setActive(false)  // Mark inactive
    Registry.erase("task1")   [ref=1]
    Timer Queue still has entry!

T2-T4: Timer thread sleeping

T5: Timer thread wakes
    entry = timerQueue.top()
    timerQueue.pop()
    
    if (entry.task->isActive()) {  // FALSE!
        // Skip moving to worker queue
    }
    // Entry destroyed, ref=0, task destroyed
    
Result: Task never executed again, memory freed automatically
```

**Benefits**:
- No O(n) search through priority queue
- No special signaling to timer thread
- Automatic cleanup via RAII
- Simple, efficient, thread-safe

---

## Conclusion

This architecture provides:

1. **Thread-Safe Scheduling**: Lock-free where possible, minimal locks where needed
2. **Noise Immunity**: Debouncing prevents chatter and flooding
3. **Flexible Behavior**: Independent channels, configurable tolerances and heartbeats
4. **Memory Safety**: RAII and shared_ptr prevent leaks
5. **Performance**: Efficient queuing, condition variables, snapshot pattern
6. **Maintainability**: Clear separation of concerns, Template Method pattern
7. **Scalability**: Worker thread pool for parallel execution

The design successfully solves all identified problems while maintaining simplicity and performance.

---

## Default Configuration Values

When creating a task, if you don't specify all configuration parameters explicitly, the framework uses these defaults:

### Constructor Signature

```cpp
TaskBase::TaskBase(const std::string& name,
                   int intervalMs,
                   int sigTolerance = 10,        // DEFAULT
                   int sigRepeat = 0,            // DEFAULT
                   int actTolerance = 10,        // DEFAULT
                   int actRepeat = 0)            // DEFAULT
```

### Default Values Table

| Parameter | Default Value | Description |
|-----------|---------------|-------------|
| `intervalMs` | **Required** | Execution interval in milliseconds (must be provided) |
| `sigTolerance` | **10** | Signal channel requires 10 consecutive "true" plans to activate |
| `sigRepeat` | **0** | Signal is single-shot (no heartbeat/periodic re-firing) |
| `allowSignal` | **true** | Signal channel gate is always open (hardcoded in constructor) |
| `actTolerance` | **10** | Action channel requires 10 consecutive "true" plans to activate |
| `actRepeat` | **0** | Action is single-shot (no heartbeat/periodic re-firing) |
| `allowAction` | **true** | Action channel gate is always open (hardcoded in constructor) |

### What These Defaults Mean

#### Signal Channel Behavior
- **Debouncing**: Requires 10 consecutive execution cycles with `wantSignal = true` before activating
- **Activation**: Fires `signal(true)` once when threshold met
- **No Heartbeat**: Since `sigRepeat = 0`, signal fires only once (single-shot)
- **Deactivation**: Drops immediately on first `wantSignal = false`
- **Safety**: Gate always open (`allowSignal = true`)

#### Action Channel Behavior
- **Debouncing**: Requires 10 consecutive execution cycles with `wantAct = true` before activating
- **Activation**: Fires `act(true)` once when threshold met
- **No Heartbeat**: Since `actRepeat = 0`, action fires only once (single-shot)
- **Deactivation**: Drops immediately on first `wantAct = false`
- **Safety**: Gate always open (`allowAction = true`)

### Timing Examples

**Example 1: Default Configuration**
```
intervalMs = 100ms
sigTolerance = 10 (default)

Minimum activation time = 10 cycles × 100ms = 1000ms (1 second)
Behavior: Must receive 10 consecutive "true" plans over 1 second to activate
```

**Example 2: Faster Activation**
```
intervalMs = 50ms
sigTolerance = 5 (custom)

Minimum activation time = 5 cycles × 50ms = 250ms
Behavior: Activates in quarter-second with sustained signal
```

**Example 3: With Heartbeat**
```
intervalMs = 100ms
sigTolerance = 10 (default)
sigRepeat = 5 (custom - enables heartbeat)

Activation: After 1 second (10 × 100ms)
Heartbeat: Every 500ms (5 × 100ms) while active
```

### How Defaults Are Applied

#### Creating Task with Defaults

```cpp
// Using SensorTask with only required parameters
auto sensor = std::make_shared<SensorTask>("sensor1", 100);

// Internally calls TaskBase constructor:
// TaskBase("sensor1", 100, 10, 0, 10, 0)
//                          ↑   ↑  ↑   ↑
//                          |   |  |   └─ actRepeat = 0
//                          |   |  └───── actTolerance = 10
//                          |   └──────── sigRepeat = 0
//                          └──────────── sigTolerance = 10
```

#### Creating Task with Custom Configuration

```cpp
// Override defaults in derived class constructor
auto sensor = std::make_shared<SensorTask>(
    "sensor1", 
    100,      // intervalMs
    5,        // sigTolerance (stricter)
    3         // sigRepeat (3-cycle heartbeat)
);
```

#### Updating Configuration After Creation

```cpp
// Method 1: Update using TaskConfig struct
scheduler.updateTask("sensor1", TaskConfig{
    100,      // intervalMs
    5,        // sigTolerance
    3,        // sigRepeat
    true,     // allowSignal
    10,       // actTolerance
    0,        // actRepeat
    false     // allowAction (disable action channel)
});

// Method 2: Update using individual parameters
scheduler.updateTask("sensor1",
    100,      // intervalMs
    5,        // sigTolerance
    3,        // sigRepeat
    true,     // allowSignal
    10,       // actTolerance
    0,        // actRepeat
    false);   // allowAction
```

### Design Rationale for Defaults

**Why tolerance = 10?**
- Provides strong noise immunity
- Balances between responsiveness and stability
- Works well for most sensor/control applications at 100ms intervals

**Why repeat = 0?**
- Single-shot is safer default (no unexpected periodic firing)
- User must explicitly enable heartbeat for continuous operation
- Prevents flooding from forgotten tasks

**Why gates = true?**
- Convenience: channels work immediately without configuration
- Safety gates should be deliberately closed, not accidentally left closed
- Matches principle of least surprise

### Common Configuration Patterns

#### Pattern 1: Fast Response Sensor
```cpp
TaskConfig{
    50,       // 50ms interval
    3,        // Quick activation (150ms)
    0,        // Single-shot
    true,     // Allow signal
    10,       // Standard action debounce
    0,        // Single-shot action
    true      // Allow action
}
```

#### Pattern 2: Stable Control Loop
```cpp
TaskConfig{
    100,      // 100ms interval
    20,       // Very stable (2 second debounce)
    10,       // 1-second heartbeat
    true,     // Allow signal
    20,       // Stable action
    10,       // 1-second action heartbeat
    true      // Allow action
}
```

#### Pattern 3: Signal-Only Task
```cpp
TaskConfig{
    200,      // 200ms interval
    5,        // Moderate debounce
    0,        // Single-shot
    true,     // Allow signal
    999,      // Action effectively disabled
    0,        // No heartbeat
    false     // Block action channel
}
```

---

## Appendix: Quick Reference

### Configuration Parameters

| Parameter | Type | Purpose | Default |
|-----------|------|---------|---------|
| intervalMs | int | Execution interval | - |
| sigTolerance | int | Signal debounce threshold | 10 |
| sigRepeat | int | Signal heartbeat interval (0=none) | 0 |
| allowSignal | bool | Signal channel gate | true |
| actTolerance | int | Action debounce threshold | 10 |
| actRepeat | int | Action heartbeat interval (0=none) | 0 |
| allowAction | bool | Action channel gate | true |

### State Variables (Per Channel)

| Variable | Type | Purpose | Access |
|----------|------|---------|--------|
| counter | int | Persistence counter | run() only |
| isActive | bool | Current state | run() only |

### Thread Synchronization

| Primitive | Protects | Lock Duration |
|-----------|----------|---------------|
| registryMutex_ | Task map | Very short |
| timerMutex_ | Priority queue | Short |
| workerMutex_ | Worker queue | Short |
| configMutex_ | Task config | Very short |
| timerCV_ | Timer sleep | - |
| workerCV_ | Worker sleep | - |

---

*End of Architecture Document*
