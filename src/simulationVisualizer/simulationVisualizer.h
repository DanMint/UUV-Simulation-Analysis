#ifndef SIMULATIONVISUALIZER_H
#define SIMULATIONVISUALIZER_H

#include <SFML/Graphics.hpp>
#include <string>
#include <vector>

#include "../mapCreation/mapCreation.h"
#include "../simulation/simulation.h"

/**
 * SimulationVisualizer
 *
 * An SFML-based live viewer that renders the simulation in real time.
 * Opens a window showing the water/land grid with all agents moving
 * step-by-step. Supports adjustable speed, pause, and skip.
 *
 * Controls:
 *   Space  - Pause / Resume
 *   + / -  - Increase / decrease animation speed
 *   Enter  - Step one frame (when paused)
 *   Esc    - Skip to end (finishes simulation, closes viewer)
 *   L      - Toggle legend overlay
 *
 * Usage (in main.cpp):
 *   Simulation sim(map, config, maxSteps);
 *   SimulationVisualizer vis(map, sim);
 *   SimResult result = vis.run();  // blocks until simulation finishes
 *
 * Headless (no animation):
 *   SimResult result = sim.run();   // as before, no window at all
 */
class SimulationVisualizer {
public:
    SimulationVisualizer(const MapCreation& map, Simulation& sim, int windowSize = 700);

    /**
     * Runs the simulation with live visualization.
     * Blocks until the simulation finishes or user presses Escape.
     * Returns the SimResult from the stepped-through simulation state.
     */
    SimResult run();

private:
    const MapCreation& m_map;
    Simulation&        m_sim;
    int                m_windowSize;
    int                m_panelHeight;
    float              m_cellSize;

    // Animation state
    bool m_paused;
    int  m_delayMs;           // delay per step in ms (10-1000)
    bool m_showLegend;

    // Trail data for drawing path histories (capped at MAX_TRAIL_POINTS per agent)
    static constexpr int MAX_TRAIL_POINTS = 2000;
    struct TrailPoint {
        int row, col;
    };
    std::vector<std::vector<TrailPoint>> m_seekerTrails;
    std::vector<std::vector<TrailPoint>> m_attackerTrails;

    // Rendering helpers
    void drawGrid(sf::RenderWindow& window) const;
    void drawTrails(sf::RenderWindow& window) const;
    void drawAgents(sf::RenderWindow& window, sf::Font* font) const;
    void drawDetectorRadii(sf::RenderWindow& window) const;
    void drawInterceptorRadii(sf::RenderWindow& window) const;
    void drawLegend(sf::RenderWindow& window, sf::Font* font) const;
    void drawUI(sf::RenderWindow& window, sf::Font* font, int step, int totalSteps) const;

    // Coordinate conversion
    bool mouseToGrid(int mouseX, int mouseY, int& outRow, int& outCol) const;

    // Colors (mirroring mapVisualizer for consistency)
    static const sf::Color WATER_COLOR;
    static const sf::Color LAND_COLOR;
    static const sf::Color SEEKER_COLOR;
    static const sf::Color TARGET_COLOR;
    static const sf::Color DETECTOR_COLOR;
    static const sf::Color DETECTOR_RADIUS_COLOR;
    static const sf::Color INTERCEPTOR_COLOR;
    static const sf::Color INTERCEPTOR_RADIUS_COLOR;
    static const sf::Color TRAIL_FADE;
    static const sf::Color GRID_LINE_COLOR;
    static const sf::Color PANEL_COLOR;
    static const sf::Color BG_COLOR;
    static const sf::Color LEGEND_BG_COLOR;

    static sf::Color attackerColor(const std::string& vehicleType);
};

#endif // SIMULATIONVISUALIZER_H
