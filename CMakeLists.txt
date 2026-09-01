cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)


# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME Bluetooth_Control_Device)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user sources here
    User/Services/display_graphics/mono_font_5x7.c
    User/Services/display_graphics/mono_font_5x7.h
    User/Services/display_graphics/mono_graphics.c
    User/Services/display_graphics/mono_graphics.h
    User/App/app.c
    User/App/app.h
    User/App/app_commands.c
    User/App/app_commands.h
    User/App/app_config.h
    User/Board/board_config.h
    User/Drivers/adxl345/adxl345.c
    User/Drivers/adxl345/adxl345.h
    User/Drivers/button_input/button_input.c
    User/Drivers/button_input/button_input.h
    User/Drivers/dht11/dht11.c
    User/Drivers/dht11/dht11.h
    User/Drivers/gpio_output/gpio_output.c
    User/Drivers/gpio_output/gpio_output.h
    User/Drivers/i2c_bus/i2c_bus.c
    User/Drivers/i2c_bus/i2c_bus.h
    User/Drivers/i2c_bus_recovery/i2c_bus_recovery.c
    User/Drivers/i2c_bus_recovery/i2c_bus_recovery.h
    User/Drivers/pwm_output/pwm_output.c
    User/Drivers/pwm_output/pwm_output.h
    User/Drivers/ssd1306/ssd1306.c
    User/Drivers/ssd1306/ssd1306.h
    User/Drivers/uart_stream/uart_stream.c
    User/Drivers/uart_stream/uart_stream.h
    User/Services/command_console/command_console.c
    User/Services/command_console/command_console.h
    User/Services/environment_display/environment_display.c
    User/Services/environment_display/environment_display.h
    User/Services/environment_feedback/environment_feedback.c
    User/Services/environment_feedback/environment_feedback.h
    User/Services/environment_monitor/environment_monitor.c
    User/Services/environment_monitor/environment_monitor.h
    User/Services/motion_display/motion_display.c
    User/Services/motion_display/motion_display.h
    User/Services/motion_monitor/motion_monitor.c
    User/Services/motion_monitor/motion_monitor.h
    User/Services/output_control/output_control.c
    User/Services/output_control/output_control.h
    User/Services/output_display/output_display.c
    User/Services/output_display/output_display.h
    User/Services/system_heartbeat/system_heartbeat.c
    User/Services/system_heartbeat/system_heartbeat.h
    User/Services/uart_log/uart_log.c
    User/Services/uart_log/uart_log.h
    User/Services/warning_feedback/warning_feedback.c
    User/Services/warning_feedback/warning_feedback.h
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined include paths
    ${CMAKE_CURRENT_SOURCE_DIR}/User/App
    ${CMAKE_CURRENT_SOURCE_DIR}/User/Board
    ${CMAKE_CURRENT_SOURCE_DIR}/User/Drivers/adxl345
    ${CMAKE_CURRENT_SOURCE_DIR}/User/Drivers/button_input
    ${CMAKE_CURRENT_SOURCE_DIR}/User/Drivers/dht11
    ${CMAKE_CURRENT_SOURCE_DIR}/User/Drivers/gpio_output
    ${CMAKE_CURRENT_SOURCE_DIR}/User/Drivers/i2c_bus
    ${CMAKE_CURRENT_SOURCE_DIR}/User/Drivers/i2c_bus_recovery
    ${CMAKE_CURRENT_SOURCE_DIR}/User/Drivers/pwm_output
    ${CMAKE_CURRENT_SOURCE_DIR}/User/Drivers/ssd1306
    ${CMAKE_CURRENT_SOURCE_DIR}/User/Drivers/uart_stream
    ${CMAKE_CURRENT_SOURCE_DIR}/User/Services/command_console
    ${CMAKE_CURRENT_SOURCE_DIR}/User/Services/display_graphics
    ${CMAKE_CURRENT_SOURCE_DIR}/User/Services/environment_display
    ${CMAKE_CURRENT_SOURCE_DIR}/User/Services/environment_feedback
    ${CMAKE_CURRENT_SOURCE_DIR}/User/Services/environment_monitor
    ${CMAKE_CURRENT_SOURCE_DIR}/User/Services/motion_display
    ${CMAKE_CURRENT_SOURCE_DIR}/User/Services/motion_monitor
    ${CMAKE_CURRENT_SOURCE_DIR}/User/Services/output_control
    ${CMAKE_CURRENT_SOURCE_DIR}/User/Services/output_display
    ${CMAKE_CURRENT_SOURCE_DIR}/User/Services/system_heartbeat
    ${CMAKE_CURRENT_SOURCE_DIR}/User/Services/uart_log
    ${CMAKE_CURRENT_SOURCE_DIR}/User/Services/warning_feedback
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx

    # Add user defined libraries
)
