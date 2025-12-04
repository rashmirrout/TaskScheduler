# Create TaskScheduler library
add_library(TaskScheduler
    task_base.cpp
    scheduler.cpp
    sensor_task.cpp
    actuator_task.cpp
    config_parser.cpp
    file_watcher.cpp
    task_factory.cpp
    config_manager.cpp
)

# Include directories
target_include_directories(TaskScheduler PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

# Link threading library
find_package(Threads REQUIRED)
target_link_libraries(TaskScheduler PUBLIC 
    Threads::Threads
    pugixml::pugixml
)

# Platform-specific settings
if(WIN32)
    target_compile_definitions(TaskScheduler PRIVATE _WIN32_WINNT=0x0601)
endif()

# Main executable
add_executable(TaskScheduler_Demo main.cpp)
target_link_libraries(TaskScheduler_Demo PRIVATE TaskScheduler)

# Set output name
set_target_properties(TaskScheduler_Demo PROPERTIES OUTPUT_NAME "main")
