cmake_minimum_required(VERSION 3.16)

# prevents git describe edge cases; optional but pragmatic
set(PROJECT_VER "0.1.0")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp32-firmware-core)

