cmake_minimum_required(VERSION 3.14)
project(UUVSim LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
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
    src/pathfinding/pathfinding.cpp
    src/simulation/simulation.cpp
    src/simulation/simResult.cpp

    src/agents/targetAgents/targetAgent.cpp

    src/agents/detectorAgents/detectorAgent.cpp
    src/agents/detectorAgents/basic/basicDetectorAgent.cpp
    src/agents/detectorAgents/medium/mediumDetectorAgent.cpp
    src/agents/detectorAgents/advanced/advancedDetectorAgent.cpp

    src/agents/interceptorAgents/interceptorAgent.cpp
    src/agents/interceptorAgents/interceptorAgent.cpp
    src/agents/interceptorAgents/basic/basicInterceptorAgent.cpp
    src/agents/interceptorAgents/medium/mediumInterceptorAgent.cpp
    src/agents/interceptorAgents/advanced/advancedInterceptorAgent.cpp

    src/agents/seekerAgents/seekerAgent.cpp
    src/agents/seekerAgents/basic/basicSeekerAgent.cpp
    src/agents/seekerAgents/fast/fastSeekerAgent.cpp
    src/agents/seekerAgents/evader/evaderSeekerAgent.cpp
)

add_executable(test_target_assignment
    tests/test_target_assignment.cpp
    src/mapCreation/mapCreation.cpp
    src/spawnConfig/spawnConfig.cpp
    src/pathfinding/pathfinding.cpp
    src/simulation/simulation.cpp
    src/simulation/simResult.cpp
    
    src/agents/targetAgents/targetAgent.cpp

    src/agents/detectorAgents/detectorAgent.cpp
    src/agents/detectorAgents/basic/basicDetectorAgent.cpp
    src/agents/detectorAgents/medium/mediumDetectorAgent.cpp
    src/agents/detectorAgents/advanced/advancedDetectorAgent.cpp

    src/agents/interceptorAgents/interceptorAgent.cpp
    src/agents/interceptorAgents/interceptorAgent.cpp
    src/agents/interceptorAgents/basic/basicInterceptorAgent.cpp
    src/agents/interceptorAgents/medium/mediumInterceptorAgent.cpp
    src/agents/interceptorAgents/advanced/advancedInterceptorAgent.cpp
    
    src/agents/seekerAgents/seekerAgent.cpp
    src/agents/seekerAgents/basic/basicSeekerAgent.cpp
    src/agents/seekerAgents/fast/fastSeekerAgent.cpp
    src/agents/seekerAgents/evader/evaderSeekerAgent.cpp
)

target_include_directories(uuv_sim PRIVATE
    src/agents

    src/agents/targetAgents

    src/agents/detectorAgents
    src/agents/detectorAgents/basic
    src/agents/detectorAgents/medium
    src/agents/detectorAgents/advanced

    src/agents/interceptorAgents
    src/agents/interceptorAgents/basic
    src/agents/interceptorAgents/medium
    src/agents/interceptorAgents/advanced

    src/agents/seekerAgents
    src/agents/seekerAgents/basic
    src/agents/seekerAgents/fast
    src/agents/seekerAgents/evader

    src/mapCreation
    src/spawnConfig
    src/mapVisualizer
    src/pathfinding
    src/simulation
)

target_include_directories(test_target_assignment PRIVATE
     src/agents

    src/agents/targetAgents

    src/agents/detectorAgents
    src/agents/detectorAgents/basic
    src/agents/detectorAgents/medium
    src/agents/detectorAgents/advanced

    src/agents/interceptorAgents
    src/agents/interceptorAgents/basic
    src/agents/interceptorAgents/medium
    src/agents/interceptorAgents/advanced

    src/agents/seekerAgents
    src/agents/seekerAgents/basic
    src/agents/seekerAgents/fast
    src/agents/seekerAgents/evader

    src/mapCreation
    src/spawnConfig
    src/mapVisualizer
    src/pathfinding
    src/simulation
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
# GDAL — use gdal-config directly (avoids Homebrew's massive
# CMake config transitive dependency resolution which hangs)
# ------------------------------------------------------------
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
message(STATUS "GDAL_CFLAGS: ${GDAL_CFLAGS}")
message(STATUS "GDAL_PREFIX: ${GDAL_PREFIX}")

separate_arguments(GDAL_CFLAGS)
target_compile_options(uuv_sim PRIVATE ${GDAL_CFLAGS})
target_link_directories(uuv_sim PRIVATE ${GDAL_PREFIX}/lib)
target_link_libraries(uuv_sim PRIVATE gdal)

target_compile_options(test_target_assignment PRIVATE ${GDAL_CFLAGS})
target_link_directories(test_target_assignment PRIVATE ${GDAL_PREFIX}/lib)
target_link_libraries(test_target_assignment PRIVATE gdal)

# Helpful on Apple Silicon if you ever accidentally mix archs
if (APPLE)
    message(STATUS "CMAKE_OSX_ARCHITECTURES: ${CMAKE_OSX_ARCHITECTURES}")
endif()