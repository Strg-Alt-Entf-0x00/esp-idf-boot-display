# ESP32-P4 LP-Core Boot Display Console Developer Example
cmake_minimum_required(VERSION 3.16)

# Add lp-boot-display component directory
set(EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/../.."
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(boot_display_console_dev)
