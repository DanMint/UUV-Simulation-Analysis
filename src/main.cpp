#include "mapCreation.h"
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Usage:\n"
                  << "  " << argv[0] << " <shapefile.shp> [cells_n]\n"
                  << "  " << argv[0] << " --cache <cache_file.txt>\n"
                  << "\nExamples:\n"
                  << "  " << argv[0] << " data/harbor.shp 100\n"
                  << "  " << argv[0] << " --cache harbor_cache.txt\n";
        return 1;
    }

    try {
        // Check if loading from cache
        std::string firstArg = argv[1];

        if (firstArg == "--cache") {
            if (argc < 3) {
                std::cerr << "Error: --cache requires a file path\n";
                return 1;
            }
            // Load from cache (no GDAL needed)
            std::cout << "Loading grid from cache...\n";
            MapCreation map = MapCreation::fromCache(argv[2]);
            map.printStats();
            map.printGrid();
        }
        else {
            // Load from shapefile
            int cellsN = (argc >= 3) ? std::stoi(argv[2]) : 100;

            std::cout << "Loading shapefile: " << firstArg << "\n";
            std::cout << "Grid resolution: " << cellsN << "x" << cellsN << "\n\n";

            MapCreation map(firstArg, cellsN);

            // Print results
            map.printStats();
            map.printGrid();

            // Save cache for future runs
            std::string cacheName = "grid_cache.txt";
            map.saveCache(cacheName);

            // Test some specific cells
            std::cout << "Sample cell checks:\n";
            int mid = cellsN / 2;
            std::cout << "  Center (" << mid << "," << mid << "): "
                      << (map.isWater(mid, mid) ? "water" : "land") << "\n";
            std::cout << "  Corner (0,0): "
                      << (map.isWater(0, 0) ? "water" : "land") << "\n";

            // Count water cells
            auto waterCells = map.getAllWaterCells();
            std::cout << "  Total water cells available for spawning: "
                      << waterCells.size() << "\n";

            for (const auto &el : map.getMgrid()) {
                for (const auto &elo : el) {
                    std::cout << elo << "";
                }
                std::cout << '\n';
            }
        }

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}