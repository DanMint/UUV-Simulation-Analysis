cmake_minimum_required(VERSION 3.14)
project(UUVSim LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Default to Release (with -O2) if no build type specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_FIND_PACKAGE_PREFER_CONFIG ON)

add_executable(uuv_sim
    src/main.cpp
    src/mapCreation/mapCreation.cpp
    src/spawnConfig/spawnConfig.cpp
    src/mapVisualizer/mapVisualizer.cpp
    src/simulationVisualizer/simulationVisualizer.cpp
    src/pathfinding/pathfinding.cpp
    src/simulation/simulation.cpp
    src/simulation/simulationRecorder.cpp
    src/simulation/simResult.cpp
    src/agents/targetAgent.cpp
    src/agents/seekerAgent.cpp
    src/agents/attackerAgent.cpp
    src/agents/detectorAgent.cpp
    src/agents/interceptorAgent.cpp
    src/agents/vehicleSpecs.cpp
)

target_include_directories(uuv_sim PRIVATE
    src/agents
    src/mapCreation
    src/spawnConfig
    src/mapVisualizer
    src/simulationVisualizer
    src/pathfinding
    src/simulation
    src/utils
)

# ------------------------------------------------------------
# SFML (Homebrew currently ships 3.x; keep fallback to 2.x)
# ------------------------------------------------------------
find_package(SFML 3 CONFIG QUIET COMPONENTS Graphics Window System)
if (SFML_FOUND)
    message(STATUS "Found SFML ${SFML_VERSION} (3.x)")
    target_link_libraries(uuv_sim PRIVATE SFML::Graphics SFML::Window SFML::System)
else()
    find_package(SFML 2 CONFIG REQUIRED COMPONENTS graphics window system)
    message(STATUS "Found SFML ${SFML_VERSION} (2.x)")
    if (TARGET SFML::Graphics)
        target_link_libraries(uuv_sim PRIVATE SFML::Graphics SFML::Window SFML::System)
    else()
        target_link_libraries(uuv_sim PRIVATE sfml-graphics sfml-window sfml-system)
    endif()
endif()

# ------------------------------------------------------------
# GDAL — prefer CMake config, fall back to gdal-config on macOS
# ------------------------------------------------------------
find_package(GDAL CONFIG QUIET)
if (GDAL_FOUND)
    message(STATUS "Found GDAL ${GDAL_VERSION} (CMake config)")
    target_link_libraries(uuv_sim PRIVATE GDAL::GDAL)
elseif(WIN32)
    message(STATUS "GDAL CMake config not found, using hardcoded path: C:/gdal")
    set(GDAL_PREFIX "C:/gdal")
    target_include_directories(uuv_sim PRIVATE "${GDAL_PREFIX}/include")
    target_link_directories(uuv_sim PRIVATE "${GDAL_PREFIX}/lib")
    target_link_libraries(uuv_sim PRIVATE gdal_i)
else()
    find_program(GDAL_CONFIG gdal-config REQUIRED)
    execute_process(
        COMMAND ${GDAL_CONFIG} --cflags
        OUTPUT_VARIABLE GDAL_CFLAGS
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    execute_process(
        COMMAND ${GDAL_CONFIG} --prefix
        OUTPUT_VARIABLE GDAL_PREFIX
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    message(STATUS "Using gdal-config: ${GDAL_CONFIG}")
    target_compile_options(uuv_sim PRIVATE ${GDAL_CFLAGS})
    target_link_directories(uuv_sim PRIVATE ${GDAL_PREFIX}/lib)
    target_link_libraries(uuv_sim PRIVATE gdal)
endif()

# Helpful on Apple Silicon if you ever accidentally mix archs
if (APPLE)
    message(STATUS "CMAKE_OSX_ARCHITECTURES: ${CMAKE_OSX_ARCHITECTURES}")
endif()

# ─── Tests ─────────────────────────────────────────────────────────────────────
enable_testing()

add_executable(test_attackerAgent
    tests/test_attackerAgent.cpp
    src/agents/attackerAgent.cpp
    src/agents/vehicleSpecs.cpp
    src/agents/seekerAgent.cpp
    src/agents/detectorAgent.cpp
    src/agents/interceptorAgent.cpp
    src/pathfinding/pathfinding.cpp
)
target_include_directories(test_attackerAgent PRIVATE
    src/agents
    src/mapCreation
    src/pathfinding
)
target_link_libraries(test_attackerAgent PRIVATE
    SFML::Graphics SFML::Window SFML::System
)
if (GDAL_FOUND)
    target_link_libraries(test_attackerAgent PRIVATE GDAL::GDAL)
else()
    target_link_directories(test_attackerAgent PRIVATE ${GDAL_PREFIX}/lib)
    target_link_libraries(test_attackerAgent PRIVATE gdal_i)
endif()

add_executable(test_simulation
    tests/test_simulation.cpp
    src/mapCreation/mapCreation.cpp
    src/spawnConfig/spawnConfig.cpp
    src/simulation/simulation.cpp
    src/simulation/simulationRecorder.cpp
    src/simulation/simResult.cpp
    src/pathfinding/pathfinding.cpp
    src/agents/targetAgent.cpp
    src/agents/seekerAgent.cpp
    src/agents/attackerAgent.cpp
    src/agents/detectorAgent.cpp
    src/agents/interceptorAgent.cpp
    src/agents/vehicleSpecs.cpp
)
target_include_directories(test_simulation PRIVATE
    src/agents
    src/mapCreation
    src/spawnConfig
    src/pathfinding
    src/simulation
    src/utils
)
target_link_libraries(test_simulation PRIVATE
    SFML::Graphics SFML::Window SFML::System
)
if (GDAL_FOUND)
    target_link_libraries(test_simulation PRIVATE GDAL::GDAL)
else()
    target_include_directories(test_simulation PRIVATE "${GDAL_PREFIX}/include")
    target_link_directories(test_simulation PRIVATE "${GDAL_PREFIX}/lib")
    target_link_libraries(test_simulation PRIVATE gdal_i)
endif()

add_executable(test_diveld_scenario
    tests/test_diveld_scenario.cpp
    src/mapCreation/mapCreation.cpp
    src/spawnConfig/spawnConfig.cpp
    src/simulation/simulation.cpp
    src/simulation/simulationRecorder.cpp
    src/simulation/simResult.cpp
    src/pathfinding/pathfinding.cpp
    src/agents/targetAgent.cpp
    src/agents/seekerAgent.cpp
    src/agents/attackerAgent.cpp
    src/agents/detectorAgent.cpp
    src/agents/interceptorAgent.cpp
    src/agents/vehicleSpecs.cpp
)
target_include_directories(test_diveld_scenario PRIVATE
    src/agents
    src/mapCreation
    src/spawnConfig
    src/pathfinding
    src/simulation
    src/utils
)
target_link_libraries(test_diveld_scenario PRIVATE
    SFML::Graphics SFML::Window SFML::System
)
if (GDAL_FOUND)
    target_link_libraries(test_diveld_scenario PRIVATE GDAL::GDAL)
else()
    target_include_directories(test_diveld_scenario PRIVATE "${GDAL_PREFIX}/include")
    target_link_directories(test_diveld_scenario PRIVATE "${GDAL_PREFIX}/lib")
    target_link_libraries(test_diveld_scenario PRIVATE gdal_i)
endif()

add_test(NAME test_attackerAgent COMMAND test_attackerAgent WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_test(NAME test_simulation COMMAND test_simulation WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_test(NAME test_diveld_scenario COMMAND test_diveld_scenario WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

add_executable(test_logger
    tests/test_logger.cpp
)
target_include_directories(test_logger PRIVATE
    src/utils
)
add_test(NAME test_logger COMMAND test_logger WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

add_executable(test_spatialGrid
    tests/test_spatialGrid.cpp
)
target_include_directories(test_spatialGrid PRIVATE
    src/utils
)
add_test(NAME test_spatialGrid COMMAND test_spatialGrid WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

add_executable(test_mapCreation
    tests/test_mapCreation.cpp
    src/mapCreation/mapCreation.cpp
    src/pathfinding/pathfinding.cpp
)
target_include_directories(test_mapCreation PRIVATE
    src/mapCreation
    src/pathfinding
)
target_link_libraries(test_mapCreation PRIVATE
    SFML::Graphics SFML::Window SFML::System
)
if (GDAL_FOUND)
    target_link_libraries(test_mapCreation PRIVATE GDAL::GDAL)
else()
    target_include_directories(test_mapCreation PRIVATE "${GDAL_PREFIX}/include")
    target_link_directories(test_mapCreation PRIVATE ${GDAL_PREFIX}/lib)
    target_link_libraries(test_mapCreation PRIVATE gdal_i)
endif()
add_test(NAME test_mapCreation COMMAND test_mapCreation WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

add_executable(test_spawnConfig
    tests/test_spawnConfig.cpp
    src/spawnConfig/spawnConfig.cpp
)
target_include_directories(test_spawnConfig PRIVATE
    src/spawnConfig
)
add_test(NAME test_spawnConfig COMMAND test_spawnConfig WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

add_executable(test_pathfinding
    tests/test_pathfinding.cpp
    src/pathfinding/pathfinding.cpp
    src/mapCreation/mapCreation.cpp
)
target_include_directories(test_pathfinding PRIVATE
    src/pathfinding
    src/mapCreation
)
target_link_libraries(test_pathfinding PRIVATE
    SFML::Graphics SFML::Window SFML::System
)
if (GDAL_FOUND)
    target_link_libraries(test_pathfinding PRIVATE GDAL::GDAL)
else()
    target_include_directories(test_pathfinding PRIVATE "${GDAL_PREFIX}/include")
    target_link_directories(test_pathfinding PRIVATE ${GDAL_PREFIX}/lib)
    target_link_libraries(test_pathfinding PRIVATE gdal_i)
endif()
add_test(NAME test_pathfinding COMMAND test_pathfinding WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

add_executable(test_simulationRecorder
    tests/test_simulationRecorder.cpp
    src/mapCreation/mapCreation.cpp
    src/spawnConfig/spawnConfig.cpp
    src/simulation/simulation.cpp
    src/simulation/simulationRecorder.cpp
    src/simulation/simResult.cpp
    src/pathfinding/pathfinding.cpp
    src/agents/targetAgent.cpp
    src/agents/seekerAgent.cpp
    src/agents/attackerAgent.cpp
    src/agents/detectorAgent.cpp
    src/agents/interceptorAgent.cpp
    src/agents/vehicleSpecs.cpp
)
target_include_directories(test_simulationRecorder PRIVATE
    src/agents
    src/mapCreation
    src/spawnConfig
    src/pathfinding
    src/simulation
    src/utils
)
target_link_libraries(test_simulationRecorder PRIVATE
    SFML::Graphics SFML::Window SFML::System
)
if (GDAL_FOUND)
    target_link_libraries(test_simulationRecorder PRIVATE GDAL::GDAL)
else()
    target_include_directories(test_simulationRecorder PRIVATE "${GDAL_PREFIX}/include")
    target_link_directories(test_simulationRecorder PRIVATE ${GDAL_PREFIX}/lib)
    target_link_libraries(test_simulationRecorder PRIVATE gdal_i)
endif()
add_test(NAME test_simulationRecorder COMMAND test_simulationRecorder WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

add_executable(test_stress
    tests/test_stress.cpp
    src/mapCreation/mapCreation.cpp
    src/spawnConfig/spawnConfig.cpp
    src/simulation/simulation.cpp
    src/simulation/simulationRecorder.cpp
    src/simulation/simResult.cpp
    src/pathfinding/pathfinding.cpp
    src/agents/targetAgent.cpp
    src/agents/seekerAgent.cpp
    src/agents/attackerAgent.cpp
    src/agents/detectorAgent.cpp
    src/agents/interceptorAgent.cpp
    src/agents/vehicleSpecs.cpp
)
target_include_directories(test_stress PRIVATE
    src/agents
    src/mapCreation
    src/spawnConfig
    src/pathfinding
    src/simulation
    src/utils
)
target_link_libraries(test_stress PRIVATE
    SFML::Graphics SFML::Window SFML::System
)
if (GDAL_FOUND)
    target_link_libraries(test_stress PRIVATE GDAL::GDAL)
else()
    target_include_directories(test_stress PRIVATE "${GDAL_PREFIX}/include")
    target_link_directories(test_stress PRIVATE ${GDAL_PREFIX}/lib)
    target_link_libraries(test_stress PRIVATE gdal_i)
endif()
add_test(NAME test_stress COMMAND test_stress WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
