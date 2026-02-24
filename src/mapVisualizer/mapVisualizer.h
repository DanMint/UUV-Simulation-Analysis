#ifndef MAPVISUALIZER_H
#define MAPVISUALIZER_H

#include <SFML/Graphics.hpp>
#include <string>
#include <vector>
#include <optional>

#include "mapCreation.h"
#include "spawnConfig.h"

/**
 * MapVisualizer (SFML 3)
 *
 * SFML-based tool for viewing the grid and placing units.
 *
 * Controls:
 *   Left click     - Place unit on water cell
 *   Right click    - Remove unit from cell
 *   S key          - Switch to Seeker mode (attacker)
 *   T key          - Switch to Target mode (defender)
 *   C key          - Clear all units
 *   Enter / Return - Save config and close
 *   Escape         - Close without saving
 */
class MapVisualizer {
public:
    MapVisualizer(const MapCreation& map, int windowSize = 700);

    /**
     * Open the window and let the user place units.
     * Returns the spawn config when the user saves (Enter).
     * Returns an empty config if the user cancels (Escape).
     */
    SpawnConfig run(const std::string& savePath = "spawn_config.json");

private:
    const MapCreation& m_map;

    int m_windowSize;
    int m_panelHeight;
    float m_cellSize;

    SpawnConfig m_config;
    std::string m_currentType;  // "seeker" or "target"

    // Colors
    static const sf::Color WATER_COLOR;
    static const sf::Color LAND_COLOR;
    static const sf::Color SEEKER_COLOR;
    static const sf::Color TARGET_COLOR;
    static const sf::Color HOVER_COLOR;
    static const sf::Color GRID_LINE_COLOR;
    static const sf::Color PANEL_COLOR;

    void drawGrid(sf::RenderWindow& window) const;
    void drawUnits(sf::RenderWindow& window, sf::Font* font) const;
    void drawHover(sf::RenderWindow& window, int hoverRow, int hoverCol) const;
    void drawStatusBar(sf::RenderWindow& window, sf::Font* font) const;
    bool mouseToGrid(int mouseX, int mouseY, int& outRow, int& outCol) const;
    void updateTitle(sf::RenderWindow& window) const;
};

#endif // MAPVISUALIZER_H