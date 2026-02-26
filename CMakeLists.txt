cmake_minimum_required(VERSION 3.14)
project(UUVSim LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_FIND_PACKAGE_PREFER_CONFIG ON)

add_executable(uuv_sim
    src/main.cpp
    src/mapCreation/mapCreation.cpp
    src/spawnConfig/spawnConfig.cpp
    src/mapVisualizer/mapVisualizer.cpp
)

target_include_directories(uuv_sim PRIVATE
    src/mapCreation
    src/spawnConfig
    src/mapVisualizer
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
# GDAL (prefer CMake config target; else use gdal-config)
# ------------------------------------------------------------

# 1) Try config package first (often provides GDAL::GDAL)
find_package(GDAL CONFIG QUIET)

if (TARGET GDAL::GDAL)
    message(STATUS "Found GDAL via CMake config target GDAL::GDAL")
    target_link_libraries(uuv_sim PRIVATE GDAL::GDAL)
else()
    # 2) Fallback: use FindGDAL module (may provide vars)
    find_package(GDAL QUIET)

    if (GDAL_FOUND)
        message(STATUS "Found GDAL via FindGDAL: includes='${GDAL_INCLUDE_DIRS}' libs='${GDAL_LIBRARIES}'")
        target_include_directories(uuv_sim PRIVATE ${GDAL_INCLUDE_DIRS})
        target_link_libraries(uuv_sim PRIVATE ${GDAL_LIBRARIES})
    else()
        # 3) Final fallback: mirror your working command using gdal-config
        find_program(GDAL_CONFIG gdal-config REQUIRED)

        execute_process(
            COMMAND ${GDAL_CONFIG} --cflags
            OUTPUT_VARIABLE GDAL_CFLAGS
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        execute_process(
            COMMAND ${GDAL_CONFIG} --libs
            OUTPUT_VARIABLE GDAL_LIBS
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )

        message(STATUS "Using gdal-config fallback")
        message(STATUS "GDAL_CFLAGS: ${GDAL_CFLAGS}")
        message(STATUS "GDAL_LIBS:   ${GDAL_LIBS}")

        separate_arguments(GDAL_CFLAGS)
        separate_arguments(GDAL_LIBS)

        target_compile_options(uuv_sim PRIVATE ${GDAL_CFLAGS})
        target_link_libraries(uuv_sim PRIVATE ${GDAL_LIBS})
    endif()
endif()

# Helpful on Apple Silicon if you ever accidentally mix archs
if (APPLE)
    message(STATUS "CMAKE_OSX_ARCHITECTURES: ${CMAKE_OSX_ARCHITECTURES}")
endif()