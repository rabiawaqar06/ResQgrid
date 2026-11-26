# File:       CMakeLists.txt
# Owner:      Sara
# Purpose:    Builds the resqgrid program, the 5 test programs and 2 tools, turns
#             warnings on.
# Depends on: CMake >= 3.16, a C++17 compiler
# Used by:    everyone ("one command builds everything")
# Milestone:  M0
#
#   cmake -S . -B build
#   cmake --build build
#   ctest --test-dir build         (runs the 5 tests)
cmake_minimum_required(VERSION 3.16)
project(ResQGrid LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

# --- warnings on for all our targets ---------------------------------------
function(resqgrid_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

# --- core library: everything except main.cpp -------------------------------
# Containers are header-only; listed so IDEs show them.
set(RESQGRID_CORE_SOURCES
    src/core/Robot.cpp
    src/core/Request.cpp
    src/core/Environment.cpp
    src/core/Router.cpp
    src/core/ScenarioLoader.cpp
    src/core/EventProcessor.cpp
    src/core/Rerouter.cpp
    src/core/Coordinator.cpp
    src/core/Movement.cpp
    src/core/Simulation.cpp
    src/output/Logger.cpp
    src/output/Statistics.cpp
)
set(RESQGRID_HEADERS
    src/containers/DynamicArray.h src/containers/LinkedList.h src/containers/Queue.h
    src/containers/MergeSort.h src/containers/HashMap.h src/containers/MinHeap.h
    src/containers/IndexedMaxHeap.h src/core/Types.h src/core/World.h
)
add_library(resqgrid_core STATIC ${RESQGRID_CORE_SOURCES} ${RESQGRID_HEADERS})
target_include_directories(resqgrid_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
resqgrid_warnings(resqgrid_core)

# --- the program ---------------------------------------------------------------
add_executable(resqgrid src/main.cpp)
target_link_libraries(resqgrid PRIVATE resqgrid_core)
resqgrid_warnings(resqgrid)

# Logs go to output/ (gitignored except .gitkeep).
file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/output)

# --- tests: each file has its own main() and is its own target -----------------
enable_testing()
foreach(test_name test_containers test_robot_request test_loader test_router test_coordinator)
    add_executable(${test_name} tests/${test_name}.cpp)
    target_link_libraries(${test_name} PRIVATE resqgrid_core)
    target_compile_definitions(${test_name} PRIVATE
        RESQGRID_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data")
    resqgrid_warnings(${test_name})
    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()

# --- tools: scenario generator (performance runs) and invariant checker -------
add_executable(gen_scenario tools/gen_scenario.cpp)
resqgrid_warnings(gen_scenario)
add_executable(check_invariants tools/check_invariants.cpp)
target_link_libraries(check_invariants PRIVATE resqgrid_core)
resqgrid_warnings(check_invariants)
