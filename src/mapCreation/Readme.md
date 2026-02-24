# MapCreation – Shapefile to Water/Land Grid Converter

`MapCreation` reads a nautical shapefile (`.shp`) containing depth polygons and converts it into a fixed-size 2D grid representing **water (0)** and **land (1)**.

The class uses **GDAL/OGR** to:

- Load polygon and multipolygon geometries from the shapefile.
- Scale geographic coordinates to a virtual pixel canvas.
- Rasterize the polygons into an `N × N` grid (default `100 × 100`).
- Classify each cell using 5-point sampling (four corners + center).
- Mark a cell as water if the majority of sample points lie inside any water polygon.

The result is a binary grid that can be used for:

- Simulations  
- Path planning  
- Visualization  
- Spatial analysis  

## Additional Features

The class also supports:

- Saving the generated grid to a cache file
- Reloading a grid from cache (without reprocessing the shapefile)
- Retrieving water/land statistics and grid metadata

This effectively performs a lightweight **vector-to-raster conversion** tailored for nautical depth data.