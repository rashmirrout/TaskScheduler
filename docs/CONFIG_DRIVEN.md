# Config-Driven Task Scheduling

This document explains the config-driven architecture that allows dynamic task management through XML configuration files with automatic file watching and hot-reloading.

## Overview

The TaskScheduler framework now supports **configuration-driven task management**, where tasks are defined in an XML file and automatically synchronized at runtime. The system watches for file changes and applies updates with intelligent debouncing.

## Architecture Components

### 1. ConfigParser
Parses XML configuration files into task configurations.

**Features:**
- Robust XML parsing using pugixml
- Validation of task configurations
- Graceful error handling
- Returns empty list on parse failure (keeps existing tasks running)

### 2. FileWatcher
Cross-platform file monitoring using polling.

**Features:**
- Polls file modification time (default: 1 second interval)
- Thread-safe callback mechanism
- Works on Windows and Linux
- No external dependencies (uses C++17 `std::filesystem`)

### 3. TaskFactory
Factory pattern for creating task instances from configurations.

**Supported Task Types:**
- `SensorTask` - Sensor monitoring tasks
- `ActuatorTask` - Actuator control tasks
- Extensible for future task types

### 4. ConfigManager
Main orchestrator that manages the entire config-driven lifecycle.

**Features:**
- **Initial Load:** Reads XML on startup and creates tasks
- **File Watching:** Monitors XML for changes
- **Debouncing:** 5-minute window to prevent rapid updates (configurable)
- **Synchronization:** Intelligently adds, updates, or removes tasks
- **Thread Safety:** All operations are thread-safe

## XML Configuration Format

```xml
<?xml version="1.0" encoding="utf-8"?>
<TaskConfigurations>
    <Task>
        <taskName>SensorA</taskName>
        <taskType>SensorTask</taskType>
        <intervalMs>1000</intervalMs>
        <sigTolerance>10</sigTolerance>
        <sigRepeat>0</sigRepeat>
        <allowSignal>true</allowSignal>
        <actTolerance>10</actTolerance>
        <actRepeat>0</actRepeat>
        <allowAction>true</allowAction>
    </Task>
    
    <Task>
        <taskName>ActuatorA</taskName>
        <taskType>ActuatorTask</taskType>
        <intervalMs>800</intervalMs>
        <sigTolerance>10</sigTolerance>
        <sigRepeat>0</sigRepeat>
        <allowSignal>true</allowSignal>
        <actTolerance>10</actTolerance>
        <actRepeat>0</actRepeat>
        <allowAction>true</allowAction>
    </Task>
</TaskConfigurations>
```

### Field Descriptions

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `taskName` | string | Yes | Unique identifier for the task |
| `taskType` | string | Yes | Task type: "SensorTask" or "ActuatorTask" |
| `intervalMs` | int | Yes | Execution interval in milliseconds (must be > 0) |
| `sigTolerance` | int | No | Signal channel tolerance (default: 10) |
| `sigRepeat` | int | No | Signal heartbeat interval (0 = single shot) |
| `allowSignal` | bool | No | Enable signal channel (default: true) |
| `actTolerance` | int | No | Action channel tolerance (default: 10) |
| `actRepeat` | int | No | Action heartbeat interval (0 = single shot) |
| `allowAction` | bool | No | Enable action channel (default: true) |

**Boolean Values:** Accepts `true`, `false`, `1`, `0`, `yes`, `no` (case-insensitive)

## Usage Example

```cpp
#include "scheduler.h"
#include "config_manager.h"

int main() {
    // Create scheduler
    Scheduler scheduler(4);  // 4 worker threads
    
    // Create config manager with custom debounce window
    ConfigManager configManager(
        scheduler, 
        "config/tasks.xml",
        std::chrono::minutes(5)  // 5-minute debounce
    );
    
    // Start config-driven management
    if (!configManager.start()) {
        std::cerr << "Failed to start ConfigManager" << std::endl;
        return 1;
    }
    
    // Tasks are now running and config is being watched
    // ...
    
    // Clean shutdown
    configManager.stop();
    return 0;
}
```

## Dynamic Configuration Updates

### How It Works

1. **File Change Detection:** FileWatcher polls the XML file every second
2. **Debounce Timer:** When change detected, starts debounce window (default: 5 minutes)
3. **Multiple Changes:** If file changes again within window, timer resets
4. **Apply Changes:** After window expires, ConfigManager:
   - Parses new XML
   - Validates configuration
   - Synchronizes tasks with scheduler

### Synchronization Logic

The system performs intelligent synchronization:

```
For each task in NEW config:
    If NOT in OLD config:
        → CREATE new task
    Else if config CHANGED:
        → UPDATE existing task
    Else:
        → No action (unchanged)

For each task in OLD config:
    If NOT in NEW config:
        → DELETE task
```

### Change Types

| Change | Action | Description |
|--------|--------|-------------|
| **ADD** | `createTask()` | New task added to XML |
| **UPDATE** | `updateTask()` | Existing task config modified |
| **REMOVE** | `stopTask()` | Task removed from XML |
| **UNCHANGED** | None | Task exists with same config |

## Error Handling

### Malformed XML
- **Behavior:** Parse fails, returns empty config list
- **Result:** Old tasks continue running unchanged
- **Logging:** Error message with parse details

### Invalid Configuration
- **Behavior:** Individual task validation fails
- **Result:** Invalid tasks skipped, valid tasks processed
- **Logging:** Validation error with task name

### File Not Found (Initial Load)
- **Behavior:** ConfigManager.start() returns false
- **Result:** No tasks created
- **Logging:** Error message with file path

### File Not Found (During Watch)
- **Behavior:** FileWatcher returns min time
- **Result:** No change detected
- **Logging:** Filesystem error message

## Debouncing Strategy

### Why Debouncing?

Prevents system thrashing when config files are being actively edited:
- Text editors may save multiple times
- Prevents partial/incomplete updates
- Reduces unnecessary task recreation
- Allows batching multiple changes

### Configuration

```cpp
// Default: 5 minutes
ConfigManager configManager(scheduler, "config.xml");

// Custom: 1 minute for testing
ConfigManager configManager(
    scheduler, 
    "config.xml",
    std::chrono::minutes(1)
);

// Custom: 30 seconds
ConfigManager configManager(
    scheduler, 
    "config.xml",
    std::chrono::seconds(30)
);
```

## Thread Safety

All components are designed for thread-safe operation:

- **ConfigParser:** Stateless, read-only operations
- **FileWatcher:** Atomic running flag, separate watch thread
- **TaskFactory:** Stateless factory methods
- **ConfigManager:** Mutex-protected config state, separate debounce thread

## Performance Considerations

### File Watching
- **Polling Interval:** 1 second (configurable in FileWatcher constructor)
- **Overhead:** Minimal - just filesystem stat() call
- **File Size:** Not a concern - only checks modification time

### Debouncing
- **Check Interval:** 1 second (internal to debounceLoop)
- **Memory:** Stores two config vectors (old + new) during sync
- **CPU:** Negligible - simple time comparison

### Synchronization
- **Complexity:** O(n) where n = number of tasks
- **Operations:** Uses unordered_map for O(1) lookups
- **Locking:** Brief mutex locks during config comparison

## Example Scenarios

### Scenario 1: Add New Task

**Edit XML:**
```xml
<Task>
    <taskName>SensorC</taskName>
    <taskType>SensorTask</taskType>
    <intervalMs>2000</intervalMs>
    ...
</Task>
```

**Output:**
```
ConfigManager: Configuration file changed, debouncing...
[Wait 5 minutes]
ConfigManager: Debounce window elapsed, applying changes...
Successfully parsed 4 task(s) from config/tasks.xml

=== ConfigManager: Synchronizing Tasks ===
  [ADD] Creating task: SensorC
=== Synchronization Complete ===
  Added: 1, Updated: 0, Removed: 0
  Total tasks: 4
```

### Scenario 2: Update Existing Task

**Edit XML:** Change SensorA interval from 1000ms to 500ms

**Output:**
```
=== ConfigManager: Synchronizing Tasks ===
  [UPDATE] Updating task: SensorA
=== Synchronization Complete ===
  Added: 0, Updated: 1, Removed: 0
  Total tasks: 3
```

### Scenario 3: Remove Task

**Edit XML:** Delete ActuatorA task block

**Output:**
```
=== ConfigManager: Synchronizing Tasks ===
  [REMOVE] Deleting task: ActuatorA
=== Synchronization Complete ===
  Added: 0, Updated: 0, Removed: 1
  Total tasks: 2
```

### Scenario 4: Malformed XML

**Edit XML:** Introduce syntax error (missing closing tag)

**Output:**
```
XML Parse Error: unexpected end of file at offset 456
ConfigManager: Failed to parse updated configuration, keeping existing tasks
```

## Best Practices

1. **Validate XML Before Saving:** Use an XML validator to catch syntax errors
2. **Test Configurations:** Verify in test environment before production
3. **Backup Configs:** Keep backups of working configurations
4. **Monitor Logs:** Watch for validation/parse errors
5. **Gradual Changes:** Make incremental changes rather than wholesale replacements
6. **Use Comments:** Document why tasks have specific configurations

## Limitations

1. **Task-Specific Parameters:** Currently, task-specific params (like SensorTask threshold) use defaults
   - **Future Enhancement:** Add optional parameters to XML
   
2. **Task Types:** Only SensorTask and ActuatorTask supported
   - **Future Enhancement:** Plugin system for custom task types
   
3. **Debounce Precision:** Minimum 1-second granularity
   - **Reason:** Debounce loop check interval

4. **No Validation Rollback:** If some tasks fail validation, valid ones still process
   - **Alternative:** Use all-or-nothing validation mode

## Troubleshooting

### Issue: Changes not applying

**Check:**
- Is file watcher running? (Look for "FileWatcher: Started watching" message)
- Has debounce window elapsed? (Default 5 minutes)
- Is XML valid? (Look for parse errors in output)

### Issue: Parse errors

**Common Causes:**
- Missing closing tags
- Invalid XML characters
- Incorrect encoding (use UTF-8)
- Missing required fields (taskName, taskType)

### Issue: Tasks not created

**Check:**
- Validation errors in logs
- Task type spelling ("SensorTask" vs "sensortask")
- Positive interval value
- Non-negative tolerance/repeat values

## Future Enhancements

1. **Hot Reload Without Debounce:** Optional immediate mode for development
2. **JSON Support:** Alternative to XML for configuration
3. **Remote Configuration:** Load config from HTTP endpoint
4. **Config History:** Track and rollback configuration changes
5. **Validation Schema:** XSD schema validation for XML
6. **Task-Specific Parameters:** Full support for all task parameters in XML
7. **Conditional Configurations:** Environment-based config selection

## See Also

- [Architecture Documentation](ARCHITECTURE.md)
- [Main README](../README.md)
- Example configuration: `config/tasks.xml`
