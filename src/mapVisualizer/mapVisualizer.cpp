#include "mapVisualizer.h"
#include <iostream>
#include <sstream>
#include <cmath>
#include <algorithm>

// ════════════════════════════════════════════════════════════════════════════════
//  COLOR CONSTANTS
// ════════════════════════════════════════════════════════════════════════════════

const sf::Color MapVisualizer::WATER_COLOR            = sf::Color(20,  50,  120);
const sf::Color MapVisualizer::LAND_COLOR             = sf::Color(50,  110, 50);
const sf::Color MapVisualizer::SEEKER_COLOR           = sf::Color(220, 30,  30);
const sf::Color MapVisualizer::TARGET_COLOR           = sf::Color(30,  100, 220);
const sf::Color MapVisualizer::DETECTOR_COLOR         = sf::Color(255, 180, 0);
const sf::Color MapVisualizer::DETECTOR_RADIUS_COLOR  = sf::Color(255, 180, 0,  40);
const sf::Color MapVisualizer::INTERCEPTOR_COLOR      = sf::Color(160, 80,  255);
const sf::Color MapVisualizer::INTERCEPTOR_RADIUS_COLOR = sf::Color(160, 80, 255, 35);
const sf::Color MapVisualizer::HOVER_COLOR            = sf::Color(255, 255, 255, 80);
const sf::Color MapVisualizer::GRID_LINE_COLOR        = sf::Color(40,  40,  40,  60);
const sf::Color MapVisualizer::PANEL_COLOR            = sf::Color(30,  30,  30);

// Attacker zones — coral/orange-red
const sf::Color MapVisualizer::ATK_ZONE_FILL   = sf::Color(220, 80,  40,  55);
const sf::Color MapVisualizer::ATK_ZONE_BORDER = sf::Color(220, 80,  40,  220);
const sf::Color MapVisualizer::ATK_ZONE_DRAG   = sf::Color(220, 80,  40,  30);

// Defender zones — teal/cyan
const sf::Color MapVisualizer::DEF_ZONE_FILL   = sf::Color(20,  180, 160, 55);
const sf::Color MapVisualizer::DEF_ZONE_BORDER = sf::Color(20,  180, 160, 220);
const sf::Color MapVisualizer::DEF_ZONE_DRAG   = sf::Color(20,  180, 160, 30);

// ════════════════════════════════════════════════════════════════════════════════
//  CONSTRUCTOR
// ════════════════════════════════════════════════════════════════════════════════

MapVisualizer::MapVisualizer(const MapCreation& map, int windowSize)
    : m_map(map),
      m_windowSize(windowSize),
      m_panelHeight(40),
      m_currentType("seeker"),
      m_zoneDrawMode(""),
      m_zoneDragging(false),
      m_zoneDragStartRow(-1),  m_zoneDragStartCol(-1),
      m_zoneDragCurrentRow(-1), m_zoneDragCurrentCol(-1),
      m_gaPrepMode(false),
      m_zoneInputState(""),
      m_pendingZone{0, 0, 0, 0},
      m_pendingFirstCount(0),
      m_typingBuffer("")
{
    m_cellSize = static_cast<float>(m_windowSize) / m_map.getCellsN();
}

// ════════════════════════════════════════════════════════════════════════════════
//  COORDINATE CONVERSION
// ════════════════════════════════════════════════════════════════════════════════

bool MapVisualizer::mouseToGrid(int mouseX, int mouseY,
                                int& outRow, int& outCol) const {
    if (mouseX < 0 || mouseX >= m_windowSize ||
        mouseY < 0 || mouseY >= m_windowSize)
        return false;

    outCol = static_cast<int>(mouseX / m_cellSize);
    outRow = static_cast<int>(mouseY / m_cellSize);
    int n  = m_map.getCellsN();
    return (outRow >= 0 && outRow < n && outCol >= 0 && outCol < n);
}

// ════════════════════════════════════════════════════════════════════════════════
//  drawGrid  (unchanged)
// ════════════════════════════════════════════════════════════════════════════════

void MapVisualizer::drawGrid(sf::RenderWindow& window) const {
    int n = m_map.getCellsN();
    const auto& grid = m_map.getGrid();
    sf::RectangleShape cell(sf::Vector2f(m_cellSize, m_cellSize));

    for (int row = 0; row < n; row++) {
        for (int col = 0; col < n; col++) {
            cell.setPosition(sf::Vector2f(col * m_cellSize, row * m_cellSize));
            cell.setFillColor(grid[row][col] == 0 ? WATER_COLOR : LAND_COLOR);
            cell.setOutlineThickness(0);
            if (m_cellSize >= 4.0f) {
                cell.setOutlineColor(GRID_LINE_COLOR);
                cell.setOutlineThickness(0.5f);
            }
            window.draw(cell);
        }
    }
}

// ════════════════════════════════════════════════════════════════════════════════
//  drawDetectorRadii  (unchanged)
// ════════════════════════════════════════════════════════════════════════════════

void MapVisualizer::drawDetectorRadii(sf::RenderWindow& window) const {
    float pixelRadius = static_cast<float>(m_config.getDetectorRadius()) * m_cellSize;
    for (const auto& unit : m_config.getUnits()) {
        if (unit.type != "detector") continue;
        sf::CircleShape c(pixelRadius);
        c.setOrigin(sf::Vector2f(pixelRadius, pixelRadius));
        c.setPosition(sf::Vector2f((unit.col + 0.5f) * m_cellSize,
                                   (unit.row + 0.5f) * m_cellSize));
        c.setFillColor(DETECTOR_RADIUS_COLOR);
        c.setOutlineColor(sf::Color(255, 180, 0, 100));
        c.setOutlineThickness(1.5f);
        c.setPointCount(48);
        window.draw(c);
    }
}

// ════════════════════════════════════════════════════════════════════════════════
//  drawInterceptorRadii  (added in last session — unchanged)
// ════════════════════════════════════════════════════════════════════════════════

void MapVisualizer::drawInterceptorRadii(sf::RenderWindow& window) const {
    float pixelRadius = static_cast<float>(m_config.getInterceptorRadius()) * m_cellSize;
    for (const auto& unit : m_config.getUnits()) {
        if (unit.type != "interceptor") continue;
        sf::CircleShape c(pixelRadius);
        c.setOrigin(sf::Vector2f(pixelRadius, pixelRadius));
        c.setPosition(sf::Vector2f((unit.col + 0.5f) * m_cellSize,
                                   (unit.row + 0.5f) * m_cellSize));
        c.setFillColor(INTERCEPTOR_RADIUS_COLOR);
        c.setOutlineColor(sf::Color(160, 80, 255, 120));
        c.setOutlineThickness(1.5f);
        c.setPointCount(48);
        window.draw(c);
    }
}

// ════════════════════════════════════════════════════════════════════════════════
//  drawZones  (replaces drawSpawnZone — now handles both sides + multiple zones)
// ════════════════════════════════════════════════════════════════════════════════

void MapVisualizer::drawZones(sf::RenderWindow& window, sf::Font* font) const {
    float hs = std::max(4.0f, m_cellSize * 0.35f);  // corner-handle size

    auto drawOneZone = [&](const SpawnZone& zone,
                           const sf::Color& fill,
                           const sf::Color& border,
                           const std::string& label) {
        float x = zone.colMin * m_cellSize;
        float y = zone.rowMin * m_cellSize;
        float w = zone.width()  * m_cellSize;
        float h = zone.height() * m_cellSize;

        // Semi-transparent fill + solid border
        sf::RectangleShape rect(sf::Vector2f(w, h));
        rect.setPosition(sf::Vector2f(x, y));
        rect.setFillColor(fill);
        rect.setOutlineColor(border);
        rect.setOutlineThickness(2.0f);
        window.draw(rect);

        // Corner handles
        const float cx[4] = { x, x + w - hs, x,         x + w - hs };
        const float cy[4] = { y, y,           y + h - hs, y + h - hs };
        for (int i = 0; i < 4; i++) {
            sf::RectangleShape handle(sf::Vector2f(hs, hs));
            handle.setPosition(sf::Vector2f(cx[i], cy[i]));
            handle.setFillColor(border);
            window.draw(handle);
        }

        // Zone label (top-left, inside the zone)
        if (font != nullptr && w > 60.f && h > 18.f) {
            sf::Text lbl(*font, label, 11);
            lbl.setFillColor(sf::Color(255, 255, 255, 200));
            lbl.setPosition(sf::Vector2f(x + hs + 3.f, y + 3.f));
            window.draw(lbl);
        }
    };

    // ── Draw all attacker zones ──
    const auto& atkZones = m_config.getAttackerZones();
    for (int i = 0; i < (int)atkZones.size(); i++) {
        std::string label = "ATK " + std::to_string(i + 1);
        if (atkZones[i].numSeekers > 0)
            label += " (" + std::to_string(atkZones[i].numSeekers) + "S)";
        drawOneZone(atkZones[i], ATK_ZONE_FILL, ATK_ZONE_BORDER, label);
    }

    // ── Draw all defender zones ──
    const auto& defZones = m_config.getDefenderZones();
    for (int i = 0; i < (int)defZones.size(); i++) {
        std::string label = "DEF " + std::to_string(i + 1);
        std::string suffix;
        if (defZones[i].numDetectors > 0)
            suffix += std::to_string(defZones[i].numDetectors) + "D";
        if (defZones[i].numInterceptors > 0) {
            if (!suffix.empty()) suffix += " ";
            suffix += std::to_string(defZones[i].numInterceptors) + "I";
        }
        if (!suffix.empty()) label += " (" + suffix + ")";
        drawOneZone(defZones[i], DEF_ZONE_FILL, DEF_ZONE_BORDER, label);
    }

    // ── Pending zone (drawn rectangle, awaiting count input) ──
    if (!m_zoneInputState.empty()) {
        const SpawnZone& z = m_pendingZone;
        float x = z.colMin * m_cellSize;
        float y = z.rowMin * m_cellSize;
        float w = z.width()  * m_cellSize;
        float h = z.height() * m_cellSize;

        bool isAtk = (m_zoneInputState == "atk_seekers");
        sf::Color fill   = isAtk ? ATK_ZONE_FILL   : DEF_ZONE_FILL;
        sf::Color border = isAtk ? ATK_ZONE_BORDER : DEF_ZONE_BORDER;

        sf::RectangleShape rect(sf::Vector2f(w, h));
        rect.setPosition(sf::Vector2f(x, y));
        rect.setFillColor(fill);
        rect.setOutlineColor(border);
        rect.setOutlineThickness(3.5f);   // thicker = "pending"
        window.draw(rect);

        if (font != nullptr && w > 60.f && h > 18.f) {
            sf::Text lbl(*font, "PENDING - type count", 11);
            lbl.setFillColor(sf::Color(255, 255, 255, 230));
            lbl.setStyle(sf::Text::Bold);
            lbl.setPosition(sf::Vector2f(x + 6.f, y + 4.f));
            window.draw(lbl);
        }
    }

    // ── Live drag preview ──
    if (m_zoneDragging && m_zoneDragStartRow >= 0 && m_zoneDragCurrentRow >= 0) {
        int r1 = std::min(m_zoneDragStartRow,   m_zoneDragCurrentRow);
        int r2 = std::max(m_zoneDragStartRow,   m_zoneDragCurrentRow);
        int c1 = std::min(m_zoneDragStartCol,   m_zoneDragCurrentCol);
        int c2 = std::max(m_zoneDragStartCol,   m_zoneDragCurrentCol);

        sf::Color dragFill   = (m_zoneDrawMode == "attacker") ? ATK_ZONE_DRAG : DEF_ZONE_DRAG;
        sf::Color dragBorder = (m_zoneDrawMode == "attacker") ? ATK_ZONE_BORDER : DEF_ZONE_BORDER;
        dragBorder.a = 160;

        float x = c1 * m_cellSize,  y = r1 * m_cellSize;
        float w = (c2 - c1 + 1) * m_cellSize;
        float h = (r2 - r1 + 1) * m_cellSize;

        sf::RectangleShape preview(sf::Vector2f(w, h));
        preview.setPosition(sf::Vector2f(x, y));
        preview.setFillColor(dragFill);
        preview.setOutlineColor(dragBorder);
        preview.setOutlineThickness(1.5f);
        window.draw(preview);
    }
}

// ════════════════════════════════════════════════════════════════════════════════
//  drawUnits  (interceptor case added; all existing cases unchanged)
// ════════════════════════════════════════════════════════════════════════════════

void MapVisualizer::drawUnits(sf::RenderWindow& window, sf::Font* font) const {
    float half = m_cellSize * 0.45f;
    if (half < 2.5f) half = 2.5f;

    for (const auto& unit : m_config.getUnits()) {
        float cx = (unit.col + 0.5f) * m_cellSize;
        float cy = (unit.row + 0.5f) * m_cellSize;

        if (unit.type == "seeker") {
            sf::CircleShape tri(half, 3);
            tri.setOrigin(sf::Vector2f(half, half));
            tri.setPosition(sf::Vector2f(cx, cy));
            tri.setFillColor(SEEKER_COLOR);
            tri.setOutlineColor(sf::Color::White);
            tri.setOutlineThickness(1.5f);
            window.draw(tri);
        }
        else if (unit.type == "target") {
            float side = half * 1.6f;
            sf::RectangleShape sq(sf::Vector2f(side, side));
            sq.setOrigin(sf::Vector2f(side / 2.f, side / 2.f));
            sq.setPosition(sf::Vector2f(cx, cy));
            sq.setFillColor(TARGET_COLOR);
            sq.setOutlineColor(sf::Color::White);
            sq.setOutlineThickness(1.5f);
            window.draw(sq);
        }
        else if (unit.type == "detector") {
            float side = half * 1.4f;
            sf::RectangleShape d(sf::Vector2f(side, side));
            d.setOrigin(sf::Vector2f(side / 2.f, side / 2.f));
            d.setPosition(sf::Vector2f(cx, cy));
            d.setRotation(sf::degrees(45.f));
            d.setFillColor(DETECTOR_COLOR);
            d.setOutlineColor(sf::Color::White);
            d.setOutlineThickness(1.5f);
            window.draw(d);
        }
        else if (unit.type == "interceptor") {
            float side = half * 1.4f;
            sf::RectangleShape d(sf::Vector2f(side, side));
            d.setOrigin(sf::Vector2f(side / 2.f, side / 2.f));
            d.setPosition(sf::Vector2f(cx, cy));
            d.setRotation(sf::degrees(45.f));
            d.setFillColor(INTERCEPTOR_COLOR);
            d.setOutlineColor(sf::Color::White);
            d.setOutlineThickness(1.5f);
            window.draw(d);
        }

        // Label letter
        if (font != nullptr && m_cellSize >= 10.0f) {
            std::string lbl;
            if      (unit.type == "seeker")      lbl = "S";
            else if (unit.type == "target")      lbl = "T";
            else if (unit.type == "detector")    lbl = "D";
            else if (unit.type == "interceptor") lbl = "I";

            unsigned int fs = static_cast<unsigned int>(m_cellSize * 0.45f);
            if (fs < 8) fs = 8;
            sf::Text txt(*font, lbl, fs);
            txt.setFillColor(sf::Color::White);
            txt.setStyle(sf::Text::Bold);
            sf::FloatRect b = txt.getLocalBounds();
            txt.setOrigin(sf::Vector2f(b.position.x + b.size.x / 2.f,
                                       b.position.y + b.size.y / 2.f));
            txt.setPosition(sf::Vector2f(cx, cy));
            window.draw(txt);
        }
    }
}

// ════════════════════════════════════════════════════════════════════════════════
//  drawHover  (zone mode suppresses cell highlight; interceptor preview added)
// ════════════════════════════════════════════════════════════════════════════════

void MapVisualizer::drawHover(sf::RenderWindow& window,
                              int hoverRow, int hoverCol) const {
    if (hoverRow < 0) return;
    if (!m_zoneDrawMode.empty()) return;  // zone drag is the visual feedback

    sf::RectangleShape h(sf::Vector2f(m_cellSize, m_cellSize));
    h.setPosition(sf::Vector2f(hoverCol * m_cellSize, hoverRow * m_cellSize));
    h.setFillColor(HOVER_COLOR);
    h.setOutlineColor(sf::Color::White);
    h.setOutlineThickness(1.0f);
    window.draw(h);

    float cx = (hoverCol + 0.5f) * m_cellSize;
    float cy = (hoverRow + 0.5f) * m_cellSize;

    if (m_currentType == "detector") {
        float r = static_cast<float>(m_config.getDetectorRadius()) * m_cellSize;
        sf::CircleShape p(r);
        p.setOrigin(sf::Vector2f(r, r));
        p.setPosition(sf::Vector2f(cx, cy));
        p.setFillColor(sf::Color(255, 180, 0, 20));
        p.setOutlineColor(sf::Color(255, 180, 0, 120));
        p.setOutlineThickness(1.0f);
        p.setPointCount(48);
        window.draw(p);
    }
    else if (m_currentType == "interceptor") {
        float r = static_cast<float>(m_config.getInterceptorRadius()) * m_cellSize;
        sf::CircleShape p(r);
        p.setOrigin(sf::Vector2f(r, r));
        p.setPosition(sf::Vector2f(cx, cy));
        p.setFillColor(sf::Color(160, 80, 255, 20));
        p.setOutlineColor(sf::Color(160, 80, 255, 120));
        p.setOutlineThickness(1.0f);
        p.setPointCount(48);
        window.draw(p);
    }
}

// ════════════════════════════════════════════════════════════════════════════════
//  drawStatusBar
// ════════════════════════════════════════════════════════════════════════════════

void MapVisualizer::drawStatusBar(sf::RenderWindow& window, sf::Font* font) const {
    sf::RectangleShape panel(sf::Vector2f(static_cast<float>(m_windowSize),
                                          static_cast<float>(m_panelHeight)));
    panel.setPosition(sf::Vector2f(0.f, static_cast<float>(m_windowSize)));
    panel.setFillColor(PANEL_COLOR);
    window.draw(panel);
    if (font == nullptr) return;

    std::ostringstream ss;
    sf::Color textColor;

    int nAtk = static_cast<int>(m_config.getAttackerZones().size());
    int nDef = static_cast<int>(m_config.getDefenderZones().size());

    if (!m_zoneInputState.empty()) {
        // ── Count-input prompt — highest priority ─────────────────────────────
        textColor = sf::Color(255, 220, 80);  // bright yellow
        std::string prompt;
        if      (m_zoneInputState == "atk_seekers")      prompt = "SEEKERS in this attacker zone";
        else if (m_zoneInputState == "def_detectors")    prompt = "DETECTORS in this defender zone";
        else if (m_zoneInputState == "def_interceptors") prompt = "INTERCEPTORS in this defender zone";

        ss << "How many " << prompt << "?  >>> "
           << (m_typingBuffer.empty() ? "_" : m_typingBuffer + "_")
           << "    [type digits | Enter=confirm | Backspace | Esc=cancel zone]";
    }
    else if (m_gaPrepMode && m_zoneDrawMode.empty()) {
        // ── GA prep mode, not currently drawing a zone ───────────────────────
        textColor = sf::Color(200, 130, 255);  // purple
        ss << "GA PREP MODE (Q to exit)  |  T=target  Z=ATK zone  X=DEF zone"
           << "  |  Targets:" << m_config.countType("target")
           << "  ATK:"        << nAtk
           << "  DEF:"        << nDef
           << "  |  Enter=save scenario for GA";
    }
    else if (m_zoneDrawMode == "attacker") {
        textColor = sf::Color(255, 140, 60);   // coral
        if (m_zoneDragging) {
            int r1 = std::min(m_zoneDragStartRow, m_zoneDragCurrentRow);
            int r2 = std::max(m_zoneDragStartRow, m_zoneDragCurrentRow);
            int c1 = std::min(m_zoneDragStartCol, m_zoneDragCurrentCol);
            int c2 = std::max(m_zoneDragStartCol, m_zoneDragCurrentCol);
            ss << "ATK ZONE DRAW  |  Dragging: " << (c2-c1+1) << "x" << (r2-r1+1)
               << " cells  |  Release to add zone";
        } else {
            ss << "ATK ZONE DRAW (Z)  |  Zones: ATK=" << nAtk << " DEF=" << nDef
               << "  |  Drag=add  RClick=remove  Shift+Z=clear ATK  Z=exit";
        }
    }
    else if (m_zoneDrawMode == "defender") {
        textColor = sf::Color(60, 210, 180);   // teal
        if (m_zoneDragging) {
            int r1 = std::min(m_zoneDragStartRow, m_zoneDragCurrentRow);
            int r2 = std::max(m_zoneDragStartRow, m_zoneDragCurrentRow);
            int c1 = std::min(m_zoneDragStartCol, m_zoneDragCurrentCol);
            int c2 = std::max(m_zoneDragStartCol, m_zoneDragCurrentCol);
            ss << "DEF ZONE DRAW  |  Dragging: " << (c2-c1+1) << "x" << (r2-r1+1)
               << " cells  |  Release to add zone";
        } else {
            ss << "DEF ZONE DRAW (X)  |  Zones: ATK=" << nAtk << " DEF=" << nDef
               << "  |  Drag=add  RClick=remove  Shift+X=clear DEF  X=exit";
        }
    }
    else {
        // Normal unit-placement mode
        std::string modeLabel;
        if      (m_currentType == "seeker")      { modeLabel = "SEEKER (S)";      textColor = sf::Color(255, 120, 120); }
        else if (m_currentType == "target")      { modeLabel = "TARGET (T)";      textColor = sf::Color(120, 160, 255); }
        else if (m_currentType == "detector")    { modeLabel = "DETECTOR (D)";    textColor = sf::Color(255, 200, 80);  }
        else if (m_currentType == "interceptor") { modeLabel = "INTERCEPTOR (I)"; textColor = sf::Color(200, 120, 255); }

        ss << "Mode: " << modeLabel
           << "  |  S:" << m_config.countType("seeker")
           << "  T:"    << m_config.countType("target")
           << "  D:"    << m_config.countType("detector")
           << "  I:"    << m_config.countType("interceptor")
           << "  DetR:" << m_config.getDetectorRadius()
           << "  IntR:" << m_config.getInterceptorRadius()
           << "  N:"    << m_config.getMaxNoiseLevel()
           << "  |  ATK zones:" << nAtk
           << "  DEF zones:"    << nDef
           << "  |  Z=ATK  X=DEF  Enter=Save";
    }

    sf::Text text(*font, ss.str(), 13);
    text.setPosition(sf::Vector2f(8.f, static_cast<float>(m_windowSize) + 10.f));
    text.setFillColor(textColor);
    window.draw(text);
}

// ════════════════════════════════════════════════════════════════════════════════
//  updateTitle
// ════════════════════════════════════════════════════════════════════════════════

void MapVisualizer::updateTitle(sf::RenderWindow& window) const {
    int nAtk = static_cast<int>(m_config.getAttackerZones().size());
    int nDef = static_cast<int>(m_config.getDefenderZones().size());

    std::ostringstream ss;
    if (m_gaPrepMode && m_zoneDrawMode.empty()) {
        ss << "UUV Spawn Tool  |  GA PREP MODE  |  T:" << m_config.countType("target")
           << "  ATK=" << nAtk << "  DEF=" << nDef;
    }
    else if (m_zoneDrawMode == "attacker") {
        ss << "UUV Spawn Tool  |  ATK ZONE DRAW  |  ATK=" << nAtk << " DEF=" << nDef;
    } else if (m_zoneDrawMode == "defender") {
        ss << "UUV Spawn Tool  |  DEF ZONE DRAW  |  ATK=" << nAtk << " DEF=" << nDef;
    } else {
        ss << "UUV Spawn Tool  |  " << m_currentType
           << "  S:" << m_config.countType("seeker")
           << "  T:" << m_config.countType("target")
           << "  D:" << m_config.countType("detector")
           << "  I:" << m_config.countType("interceptor")
           << "  ATK:" << nAtk << "  DEF:" << nDef;
    }
    window.setTitle(ss.str());
}

// ════════════════════════════════════════════════════════════════════════════════
//  MAIN LOOP
// ════════════════════════════════════════════════════════════════════════════════

SpawnConfig MapVisualizer::run(const std::string& savePath) {
    // ── Window ────────────────────────────────────────────────────────────────
    sf::RenderWindow window(
        sf::VideoMode(sf::Vector2u(static_cast<unsigned int>(m_windowSize),
                                   static_cast<unsigned int>(m_windowSize + m_panelHeight))),
        "UUV Spawn Tool",
        sf::Style::Close);
    window.setFramerateLimit(30);

    // ── Font ──────────────────────────────────────────────────────────────────
    sf::Font  font;
    sf::Font* fontPtr = nullptr;
    const std::string fontPaths[] = {
        "/System/Library/Fonts/Menlo.ttc",
        "/System/Library/Fonts/SFNSMono.ttf",
        "/System/Library/Fonts/Helvetica.ttc",
        "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
        "/usr/share/fonts/TTF/DejaVuSansMono.ttf",
        "C:\\Windows\\Fonts\\consola.ttf",
    };
    for (const auto& p : fontPaths)
        if (font.openFromFile(p)) { fontPtr = &font; break; }

    if (!fontPtr)
        std::cout << "Warning: no system font found. Status bar disabled.\n"
                  << "Keys: S T D I=unit  Z=ATK zone  X=DEF zone  +/-=DetR  {/}=IntR  [/]=Noise  "
                  << "C=clear units  Shift+Z/X=clear zones  Enter=save  Esc=cancel\n";

    int hoverRow = -1, hoverCol = -1;

    // ── Event loop ────────────────────────────────────────────────────────────
    while (window.isOpen()) {
        while (auto event = window.pollEvent()) {

            if (event->is<sf::Event::Closed>()) {
                window.close();
                return SpawnConfig();
            }

            // ── Keyboard ─────────────────────────────────────────────────────
            if (const auto* k = event->getIf<sf::Event::KeyPressed>()) {
                bool shift = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LShift)
                          || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RShift);

                // ── Zone count input — takes priority over all other keys ────
                if (!m_zoneInputState.empty()) {
                    if (k->code == sf::Keyboard::Key::Enter) {
                        int count = 0;
                        try { if (!m_typingBuffer.empty()) count = std::stoi(m_typingBuffer); }
                        catch (...) { count = 0; }

                        if (m_zoneInputState == "atk_seekers") {
                            m_config.addAttackerZone(
                                m_pendingZone.rowMin, m_pendingZone.colMin,
                                m_pendingZone.rowMax, m_pendingZone.colMax,
                                count);
                            std::cout << "Added ATK zone "
                                      << m_config.getAttackerZones().size()
                                      << " with " << count << " seeker(s)\n";
                            m_zoneInputState = "";
                            m_typingBuffer.clear();
                        }
                        else if (m_zoneInputState == "def_detectors") {
                            m_pendingFirstCount = count;
                            m_zoneInputState    = "def_interceptors";
                            m_typingBuffer.clear();
                            std::cout << "How many INTERCEPTORS in this defender zone? "
                                      << "(type digits, Enter to confirm, Esc to cancel)\n";
                        }
                        else if (m_zoneInputState == "def_interceptors") {
                            m_config.addDefenderZone(
                                m_pendingZone.rowMin, m_pendingZone.colMin,
                                m_pendingZone.rowMax, m_pendingZone.colMax,
                                m_pendingFirstCount, count);
                            std::cout << "Added DEF zone "
                                      << m_config.getDefenderZones().size()
                                      << " with " << m_pendingFirstCount << " detector(s) and "
                                      << count << " interceptor(s)\n";
                            m_zoneInputState = "";
                            m_typingBuffer.clear();
                        }
                        updateTitle(window);
                    }
                    else if (k->code == sf::Keyboard::Key::Escape) {
                        std::cout << "Zone cancelled.\n";
                        m_zoneInputState = "";
                        m_typingBuffer.clear();
                        updateTitle(window);
                    }
                    else if (k->code == sf::Keyboard::Key::Backspace) {
                        if (!m_typingBuffer.empty()) m_typingBuffer.pop_back();
                    }
                    // Block every other key while we're collecting count input
                    continue;
                }

                // ── Unit modes — exit zone draw if active ─────────────────────
                if (k->code == sf::Keyboard::Key::S) {
                    if (m_gaPrepMode) {
                        std::cout << "Seeker placement disabled in GA prep mode. "
                                  << "The GA will spawn seekers within attacker zones.\n";
                    } else {
                        m_currentType = "seeker";
                        m_zoneDrawMode = "";  m_zoneDragging = false;
                        updateTitle(window);
                    }
                }
                else if (k->code == sf::Keyboard::Key::T) {
                    m_currentType = "target";
                    m_zoneDrawMode = "";  m_zoneDragging = false;
                    updateTitle(window);
                }
                else if (k->code == sf::Keyboard::Key::D) {
                    if (m_gaPrepMode) {
                        std::cout << "Detector placement disabled in GA prep mode.\n";
                    } else {
                        m_currentType = "detector";
                        m_zoneDrawMode = "";  m_zoneDragging = false;
                        updateTitle(window);
                    }
                }
                else if (k->code == sf::Keyboard::Key::I) {
                    if (m_gaPrepMode) {
                        std::cout << "Interceptor placement disabled in GA prep mode.\n";
                    } else {
                        m_currentType = "interceptor";
                        m_zoneDrawMode = "";  m_zoneDragging = false;
                        updateTitle(window);
                    }
                }

                // ── Q — toggle GA prep mode ───────────────────────────────────
                else if (k->code == sf::Keyboard::Key::Q) {
                    m_gaPrepMode = !m_gaPrepMode;
                    if (m_gaPrepMode) {
                        // Switch to target mode if we were in a disabled mode
                        if (m_currentType == "seeker"   ||
                            m_currentType == "detector" ||
                            m_currentType == "interceptor") {
                            m_currentType = "target";
                        }
                        std::cout << "GA PREP MODE ON  —  only targets and zones can be "
                                  << "placed. Press Q to exit.\n";
                    } else {
                        std::cout << "GA PREP MODE OFF  —  all unit types available again.\n";
                    }
                    updateTitle(window);
                }

                // ── Z — attacker zone mode ────────────────────────────────────
                else if (k->code == sf::Keyboard::Key::Z) {
                    if (shift) {
                        // Shift+Z: clear ALL attacker zones
                        m_config.clearAttackerZones();
                        m_zoneDragging = false;
                        std::cout << "All attacker zones cleared.\n";
                    } else if (m_zoneDrawMode == "attacker") {
                        // Z again: exit attacker zone mode (zones kept)
                        m_zoneDrawMode = "";
                        m_zoneDragging = false;
                        std::cout << "Exited attacker zone mode.\n";
                    } else {
                        // Enter attacker zone mode
                        m_zoneDrawMode = "attacker";
                        m_zoneDragging = false;
                        std::cout << "Attacker zone mode — drag to add zones, "
                                  << "RClick to remove, Shift+Z to clear all.\n";
                    }
                    updateTitle(window);
                }

                // ── X — defender zone mode ────────────────────────────────────
                else if (k->code == sf::Keyboard::Key::X) {
                    if (shift) {
                        // Shift+X: clear ALL defender zones
                        m_config.clearDefenderZones();
                        m_zoneDragging = false;
                        std::cout << "All defender zones cleared.\n";
                    } else if (m_zoneDrawMode == "defender") {
                        // X again: exit defender zone mode (zones kept)
                        m_zoneDrawMode = "";
                        m_zoneDragging = false;
                        std::cout << "Exited defender zone mode.\n";
                    } else {
                        // Enter defender zone mode
                        m_zoneDrawMode = "defender";
                        m_zoneDragging = false;
                        std::cout << "Defender zone mode — drag to add zones, "
                                  << "RClick to remove, Shift+X to clear all.\n";
                    }
                    updateTitle(window);
                }

                // ── C — clear all units (zones intentionally preserved) ───────
                else if (k->code == sf::Keyboard::Key::C) {
                    m_config.clear();
                    updateTitle(window);
                }

                // ── Detector sensing radius  + / - ────────────────────────────
                else if (k->code == sf::Keyboard::Key::Equal ||
                         k->code == sf::Keyboard::Key::Add) {
                    m_config.setDetectorRadius(m_config.getDetectorRadius() + 0.5);
                    std::cout << "Detector sensing radius: " << m_config.getDetectorRadius() << "\n";
                    updateTitle(window);
                }
                else if (k->code == sf::Keyboard::Key::Hyphen ||
                         k->code == sf::Keyboard::Key::Subtract) {
                    double r = m_config.getDetectorRadius();
                    if (r > 0.5) m_config.setDetectorRadius(r - 0.5);
                    std::cout << "Detector sensing radius: " << m_config.getDetectorRadius() << "\n";
                    updateTitle(window);
                }

                // ── Interceptor kill radius  { / }  (Shift + [ / ]) ──────────
                else if (k->code == sf::Keyboard::Key::RBracket && shift) {
                    m_config.setInterceptorRadius(m_config.getInterceptorRadius() + 0.5);
                    std::cout << "Interceptor kill radius: " << m_config.getInterceptorRadius() << "\n";
                    updateTitle(window);
                }
                else if (k->code == sf::Keyboard::Key::LBracket && shift) {
                    double r = m_config.getInterceptorRadius();
                    if (r > 0.5) m_config.setInterceptorRadius(r - 0.5);
                    std::cout << "Interceptor kill radius: " << m_config.getInterceptorRadius() << "\n";
                    updateTitle(window);
                }

                // ── Noise  [ / ]  (without Shift) ────────────────────────────
                else if (k->code == sf::Keyboard::Key::RBracket) {
                    m_config.setMaxNoiseLevel(m_config.getMaxNoiseLevel() + 0.1);
                    std::cout << "Max noise: " << m_config.getMaxNoiseLevel() << "\n";
                    updateTitle(window);
                }
                else if (k->code == sf::Keyboard::Key::LBracket) {
                    double n = m_config.getMaxNoiseLevel();
                    if (n > 0.0) m_config.setMaxNoiseLevel(n - 0.1);
                    std::cout << "Max noise: " << m_config.getMaxNoiseLevel() << "\n";
                    updateTitle(window);
                }

                // ── Save ─────────────────────────────────────────────────────
                else if (k->code == sf::Keyboard::Key::Enter) {
                    bool hasSomething = m_config.totalUnits()    > 0
                                     || m_config.hasAttackerZones()
                                     || m_config.hasDefenderZones();
                    if (hasSomething) {
                        if (!savePath.empty()) m_config.saveJSON(savePath);
                        m_config.printSummary();
                    } else {
                        std::cout << "Nothing placed. Nothing saved.\n";
                    }
                    window.close();
                    return m_config;
                }
                else if (k->code == sf::Keyboard::Key::Escape) {
                    window.close();
                    return SpawnConfig();
                }
            }

            // ── Text input (for digits while collecting zone counts) ──────────
            if (const auto* txt = event->getIf<sf::Event::TextEntered>()) {
                if (!m_zoneInputState.empty()) {
                    char32_t ch = txt->unicode;
                    if (ch >= U'0' && ch <= U'9' && m_typingBuffer.size() < 5) {
                        m_typingBuffer += static_cast<char>(ch);
                    }
                }
            }

            // ── Mouse movement ────────────────────────────────────────────────
            if (const auto* mv = event->getIf<sf::Event::MouseMoved>()) {
                mouseToGrid(mv->position.x, mv->position.y, hoverRow, hoverCol);
                if (m_zoneDragging) {
                    int r, c;
                    if (mouseToGrid(mv->position.x, mv->position.y, r, c)) {
                        m_zoneDragCurrentRow = r;
                        m_zoneDragCurrentCol = c;
                    }
                }
            }

            // ── Mouse press ───────────────────────────────────────────────────
            if (const auto* btn = event->getIf<sf::Event::MouseButtonPressed>()) {
                // Block clicks while we're collecting zone counts
                if (!m_zoneInputState.empty()) continue;

                int clickRow, clickCol;
                if (!mouseToGrid(btn->position.x, btn->position.y, clickRow, clickCol))
                    continue;

                if (btn->button == sf::Mouse::Button::Left) {
                    if (!m_zoneDrawMode.empty()) {
                        // Start drag to add a new zone
                        if (!m_zoneDragging) {
                            m_zoneDragStartRow   = clickRow;
                            m_zoneDragStartCol   = clickCol;
                            m_zoneDragCurrentRow = clickRow;
                            m_zoneDragCurrentCol = clickCol;
                            m_zoneDragging       = true;
                        }
                    } else {
                        // Place unit (existing behaviour)
                        if (m_map.isWater(clickRow, clickCol)) {
                            if (!m_config.addUnit(m_currentType, clickRow, clickCol)) {
                                m_config.removeUnit(clickRow, clickCol);
                                m_config.addUnit(m_currentType, clickRow, clickCol);
                            }
                        }
                        updateTitle(window);
                    }
                }
                else if (btn->button == sf::Mouse::Button::Right) {
                    if (!m_zoneDrawMode.empty()) {
                        // Remove zone containing this cell
                        bool removed = false;
                        if (m_zoneDrawMode == "attacker") {
                            removed = m_config.removeAttackerZoneContaining(clickRow, clickCol);
                            if (removed) std::cout << "Removed attacker zone.\n";
                        } else if (m_zoneDrawMode == "defender") {
                            removed = m_config.removeDefenderZoneContaining(clickRow, clickCol);
                            if (removed) std::cout << "Removed defender zone.\n";
                        }
                        if (!removed) std::cout << "No zone found at that cell.\n";
                        updateTitle(window);
                    } else {
                        // Remove unit (existing behaviour)
                        m_config.removeUnit(clickRow, clickCol);
                        updateTitle(window);
                    }
                }
            }

            // ── Mouse release — capture drag rectangle, prompt for unit counts ────
            if (const auto* rel = event->getIf<sf::Event::MouseButtonReleased>()) {
                if (rel->button == sf::Mouse::Button::Left && m_zoneDragging) {
                    int r1 = std::min(m_zoneDragStartRow,   m_zoneDragCurrentRow);
                    int r2 = std::max(m_zoneDragStartRow,   m_zoneDragCurrentRow);
                    int c1 = std::min(m_zoneDragStartCol,   m_zoneDragCurrentCol);
                    int c2 = std::max(m_zoneDragStartCol,   m_zoneDragCurrentCol);

                    if (r1 != r2 || c1 != c2) {
                        // Store the rectangle and enter count-input state.
                        // The zone is only added to the config once the user
                        // commits the counts with Enter.
                        m_pendingZone.rowMin = r1;  m_pendingZone.colMin = c1;
                        m_pendingZone.rowMax = r2;  m_pendingZone.colMax = c2;
                        m_pendingZone.numSeekers      = 0;
                        m_pendingZone.numDetectors    = 0;
                        m_pendingZone.numInterceptors = 0;
                        m_typingBuffer.clear();
                        m_pendingFirstCount = 0;

                        if (m_zoneDrawMode == "attacker") {
                            m_zoneInputState = "atk_seekers";
                            std::cout << "How many SEEKERS in this attacker zone? "
                                      << "(type digits, Enter to confirm, Esc to cancel)\n";
                        } else if (m_zoneDrawMode == "defender") {
                            m_zoneInputState = "def_detectors";
                            std::cout << "How many DETECTORS in this defender zone? "
                                      << "(type digits, Enter to confirm, Esc to cancel)\n";
                        }
                    }
                    m_zoneDragging = false;
                    updateTitle(window);
                }
            }

        } // end event poll

        // ── Render ────────────────────────────────────────────────────────────
        window.clear(sf::Color::Black);
        drawGrid(window);
        drawDetectorRadii(window);
        drawInterceptorRadii(window);
        drawZones(window, fontPtr);    // draw zones under units
        drawUnits(window, fontPtr);
        drawHover(window, hoverRow, hoverCol);
        drawStatusBar(window, fontPtr);
        window.display();
    }

    return m_config;
}