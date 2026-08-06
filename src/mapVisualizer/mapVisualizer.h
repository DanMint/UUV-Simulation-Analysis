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
 * ── Unit placement ──────────────────────────────────────────────────
 *   S / T / D / I   Select a category (Seeker, Target, Detector, Interceptor)
 *   B               Select Basic type
 *   F               Select Fast type (Seeker category only)
 *   E               Select Evader type (Seeker category only)
 *   Left click      Place the selected category/type on a water cell
 *   Right click     Remove unit
 *   + / -           Adjust detector sensing radius
 *   { / }           Adjust interceptor kill radius  (Shift+[ / Shift+])
 *   [ / ]           Adjust noise level
 *   C               Clear all units  (zones are preserved)
 *   Enter           Save scenario and return
 *   Escape          Cancel
 *
 * Examples:
 *   S, then B, then left click -> category="seeker", type="basic"
 *   S, then F, then left click -> category="seeker", type="fast"
 *   S, then E, then left click -> category="seeker", type="evader"
 *
 * ── GA preparation mode ─────────────────────────────────────────────
 *   Q               Toggle GA prep mode on/off
 *                   In GA prep mode the S, D, I keys are inert. The
 *                   user only places targets and draws zones. The
 *                   Python GA fills in seekers (and later detectors /
 *                   interceptors) within the zones at evaluation time.
 *
 * ── Zone drawing ────────────────────────────────────────────────────
 *   Z               Enter / exit attacker zone draw mode
 *   X               Enter / exit defender zone draw mode
 *   Left drag       Add a zone rectangle  (while in zone mode)
 *   Right click     Remove zone under cursor  (while in zone mode)
 *   Shift + Z       Clear ALL attacker zones
 *   Shift + X       Clear ALL defender zones
 *
 * Each drag adds a new zone; zones accumulate until explicitly removed.
 * Pressing S/T/D/I exits zone mode without clearing any zones.
 */
class MapVisualizer {
public:
    MapVisualizer(const MapCreation& map, int windowSize = 700);

    SpawnConfig run(const std::string& savePath = "spawn_config.json");

private:
    const MapCreation& m_map;

    int   m_windowSize;
    int   m_panelHeight;
    float m_cellSize;

    SpawnConfig m_config;

    // Broad role selected with S/T/D/I.
    // Empty means no category is selected.
    std::string m_currentCategory;

    // Concrete implementation selected after the category.
    // Empty means the user must select a type; B selects "basic"; seeker mode also supports F="fast" and E="evader".
    std::string m_currentUnitType;

    // ── Zone draw state ──────────────────────────────────────────────
    // m_zoneDrawMode: "" = no zone mode, "attacker" or "defender"
    std::string m_zoneDrawMode;
    bool m_zoneDragging;
    int  m_zoneDragStartRow;
    int  m_zoneDragStartCol;
    int  m_zoneDragCurrentRow;
    int  m_zoneDragCurrentCol;

    // ── GA-prep mode ─────────────────────────────────────────────────
    bool m_gaPrepMode;

    // ── Zone count input state ───────────────────────────────────────
    std::string m_zoneInputState;
    SpawnZone   m_pendingZone;
    int         m_pendingFirstCount;
    std::string m_typingBuffer;

    // ── Colors ───────────────────────────────────────────────────────
    static const sf::Color WATER_COLOR;
    static const sf::Color LAND_COLOR;
    static const sf::Color SEEKER_COLOR;
    static const sf::Color TARGET_COLOR;
    static const sf::Color DETECTOR_COLOR;
    static const sf::Color DETECTOR_RADIUS_COLOR;
    static const sf::Color INTERCEPTOR_COLOR;
    static const sf::Color INTERCEPTOR_RADIUS_COLOR;
    static const sf::Color HOVER_COLOR;
    static const sf::Color GRID_LINE_COLOR;
    static const sf::Color PANEL_COLOR;
    static const sf::Color ATK_ZONE_FILL;
    static const sf::Color ATK_ZONE_BORDER;
    static const sf::Color ATK_ZONE_DRAG;
    static const sf::Color DEF_ZONE_FILL;
    static const sf::Color DEF_ZONE_BORDER;
    static const sf::Color DEF_ZONE_DRAG;

    // ── Drawing ──────────────────────────────────────────────────────
    void drawGrid(sf::RenderWindow& window) const;
    void drawDetectorRadii(sf::RenderWindow& window) const;
    void drawInterceptorRadii(sf::RenderWindow& window) const;
    void drawZones(sf::RenderWindow& window, sf::Font* font) const;
    void drawUnits(sf::RenderWindow& window, sf::Font* font) const;
    void drawHover(sf::RenderWindow& window, int hoverRow, int hoverCol) const;
    void drawStatusBar(sf::RenderWindow& window, sf::Font* font) const;
    bool mouseToGrid(int mouseX, int mouseY, int& outRow, int& outCol) const;
    void updateTitle(sf::RenderWindow& window) const;
};

#endif // MAPVISUALIZER_H