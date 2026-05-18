cmake_minimum_required(VERSION 3.22)
project(StratumPlugins VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(JUCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../vst-host/JUCE")
if(NOT EXISTS "${JUCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "JUCE not found at ${JUCE_DIR}")
endif()

add_subdirectory("${JUCE_DIR}" "${CMAKE_BINARY_DIR}/JUCE")

set(STRATUM_PLUGIN_INSTALL_DIR "${CMAKE_CURRENT_LIST_DIR}/_installed/VST3")

add_subdirectory(StratumPiano)
