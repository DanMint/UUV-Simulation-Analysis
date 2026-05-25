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
 *   S / T / D / I   Switch unit mode (Seeker, Target, Detector, Interceptor)
 *   Left click       Place unit on water cell
 *   Right click      Remove unit
 *   + / -            Adjust detector sensing radius
 *   { / }            Adjust interceptor kill radius  (Shift+[ / Shift+])
 *   [ / ]            Adjust noise level
 *   C                Clear all units  (zones are preserved)
 *   Enter            Save scenario and return
 *   Escape           Cancel
 *
 * ── GA preparation mode (new) ───────────────────────────────────────
 *   Q                Toggle GA prep mode on/off
 *                    In GA prep mode the S, D, I keys are inert — the
 *                    user only places targets and draws zones. The
 *                    Python GA fills in seekers (and later detectors /
 *                    interceptors) within the zones at evaluation time.
 *
 * ── Zone drawing ────────────────────────────────────────────────────
 *   Z                Enter / exit attacker zone draw mode
 *   X                Enter / exit defender zone draw mode
 *   Left drag        Add a zone rectangle  (while in zone mode)
 *   Right click      Remove zone under cursor  (while in zone mode)
 *   Shift + Z        Clear ALL attacker zones
 *   Shift + X        Clear ALL defender zones
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
    std::string m_currentType;     // "seeker" | "target" | "detector" | "interceptor"

    // ── Zone draw state ──────────────────────────────────────────────
    // m_zoneDrawMode: "" = no zone mode, "attacker" or "defender"
    std::string m_zoneDrawMode;
    bool m_zoneDragging;
    int  m_zoneDragStartRow;
    int  m_zoneDragStartCol;
    int  m_zoneDragCurrentRow;
    int  m_zoneDragCurrentCol;

    // ── GA-prep mode ─────────────────────────────────────────────────
    // When true, the seeker / detector / interceptor keys are inert.
    // The user only places targets and draws zones; the GA fills in
    // the rest later. Toggled with the Q key.
    bool m_gaPrepMode;

    // ── Zone count input state ───────────────────────────────────────
    // After the user releases a zone drag, the zone is "pending" while
    // we collect unit counts from the keyboard.
    //   ""                  -> not collecting input
    //   "atk_seekers"       -> attacker zone, awaiting seeker count
    //   "def_detectors"     -> defender zone, awaiting detector count
    //   "def_interceptors"  -> defender zone, awaiting interceptor count
    std::string m_zoneInputState;
    SpawnZone   m_pendingZone;        // rectangle waiting for counts
    int         m_pendingFirstCount;  // detectors count, held between the two defender prompts
    std::string m_typingBuffer;       // digits typed so far

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
    // Attacker zone — coral / orange-red
    static const sf::Color ATK_ZONE_FILL;
    static const sf::Color ATK_ZONE_BORDER;
    static const sf::Color ATK_ZONE_DRAG;
    // Defender zone — teal / cyan
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