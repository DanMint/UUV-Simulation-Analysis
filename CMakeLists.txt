cmake_minimum_required(VERSION 3.14)
project(UUVSim LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Find GDAL ──────────────────────────────────────────────────────
# GDAL provides the OGR library for reading shapefiles.
# Install GDAL first:
#   Ubuntu/Debian:  sudo apt install libgdal-dev
#   macOS (brew):   brew install gdal
#   Windows (vcpkg): vcpkg install gdal
find_package(GDAL REQUIRED)

# ── Build the executable ───────────────────────────────────────────
add_executable(uuv_sim
    src/main.cpp
    src/mapCreation/MapCreation.cpp
)

target_include_directories(uuv_sim PRIVATE ${GDAL_INCLUDE_DIRS})
target_link_libraries(uuv_sim PRIVATE ${GDAL_LIBRARIES})

# Print GDAL info during configuration (helpful for debugging)
message(STATUS "GDAL include: ${GDAL_INCLUDE_DIRS}")
message(STATUS "GDAL libraries: ${GDAL_LIBRARIES}")