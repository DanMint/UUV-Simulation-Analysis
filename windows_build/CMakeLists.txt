cmake_minimum_required(VERSION 3.14)
project(UUVSim LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_FIND_PACKAGE_PREFER_CONFIG ON)

add_executable(uuv_sim
    ../src/main.cpp
    ../src/mapCreation/mapCreation.cpp
    ../src/spawnConfig/spawnConfig.cpp
    ../src/mapVisualizer/mapVisualizer.cpp
    ../src/pathfinding/pathfinding.cpp
    ../src/simulation/simulation.cpp
    ../src/simulation/simResult.cpp
    ../src/agents/targetAgent.cpp
    ../src/agents/seekerAgent.cpp
    ../src/agents/attackerAgent.cpp
    ../src/agents/detectorAgent.cpp
    ../src/agents/interceptorAgent.cpp
)

target_include_directories(uuv_sim PRIVATE
    ../src/agents
    ../src/mapCreation
    ../src/spawnConfig
    ../src/mapVisualizer
    ../src/pathfinding
    ../src/simulation
)

find_package(SFML 3 CONFIG QUIET COMPONENTS Graphics Window System)
if (SFML_FOUND)
    target_link_libraries(uuv_sim PRIVATE SFML::Graphics SFML::Window SFML::System)
else()
    find_package(SFML 2 CONFIG REQUIRED COMPONENTS graphics window system)
    if (TARGET SFML::Graphics)
        target_link_libraries(uuv_sim PRIVATE SFML::Graphics SFML::Window SFML::System)
    else()
        target_link_libraries(uuv_sim PRIVATE sfml-graphics sfml-window sfml-system)
    endif()
endif()

set(VCPKG "C:/Users/youar/Downloads/Research-UUV-Defender-Agent1-Demo-main/vcpkg/installed/x64-windows")
target_include_directories(uuv_sim PRIVATE "${VCPKG}/include")
target_link_directories(uuv_sim PRIVATE "${VCPKG}/lib")
target_link_libraries(uuv_sim PRIVATE gdal)