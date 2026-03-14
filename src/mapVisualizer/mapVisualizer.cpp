#include "mapVisualizer.h"
#include <iostream>
#include <sstream>
#include <cmath>

// ════════════════════════════════════════════════════════════════════════════════
//  COLORS
// ════════════════════════════════════════════════════════════════════════════════

const sf::Color MapVisualizer::WATER_COLOR            = sf::Color(20,  50, 120);
const sf::Color MapVisualizer::LAND_COLOR             = sf::Color(50,  110, 50);
const sf::Color MapVisualizer::SEEKER_COLOR           = sf::Color(220, 30,  30);
const sf::Color MapVisualizer::TARGET_COLOR           = sf::Color(30,  100, 220);
const sf::Color MapVisualizer::DETECTOR_COLOR         = sf::Color(255, 180, 0);
const sf::Color MapVisualizer::DETECTOR_RADIUS_COLOR  = sf::Color(255, 180, 0, 40);
const sf::Color MapVisualizer::HOVER_COLOR            = sf::Color(255, 255, 255, 80);
const sf::Color MapVisualizer::GRID_LINE_COLOR        = sf::Color(40,  40,  40, 60);
const sf::Color MapVisualizer::PANEL_COLOR            = sf::Color(30,  30,  30);

// ════════════════════════════════════════════════════════════════════════════════
//  CONSTRUCTOR
// ════════════════════════════════════════════════════════════════════════════════

MapVisualizer::MapVisualizer(const MapCreation& map, int windowSize)
    : m_map(map),
      m_windowSize(windowSize),
      m_panelHeight(40),
      m_currentType("seeker")
{
    m_cellSize = static_cast<float>(m_windowSize) / m_map.getCellsN();
}

// ════════════════════════════════════════════════════════════════════════════════
//  COORDINATE CONVERSION
// ════════════════════════════════════════════════════════════════════════════════

bool MapVisualizer::mouseToGrid(int mouseX, int mouseY, int& outRow, int& outCol) const {
    if (mouseX < 0 || mouseX >= m_windowSize ||
        mouseY < 0 || mouseY >= m_windowSize) {
        return false;
    }
    outCol = static_cast<int>(mouseX / m_cellSize);
    outRow = static_cast<int>(mouseY / m_cellSize);

    int n = m_map.getCellsN();
    if (outRow < 0 || outRow >= n || outCol < 0 || outCol >= n) {
        return false;
    }
    return true;
}

// ════════════════════════════════════════════════════════════════════════════════
//  DRAWING
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

void MapVisualizer::drawDetectorRadii(sf::RenderWindow& window) const {
    double radius = m_config.getDetectorRadius();

    for (const auto& unit : m_config.getUnits()) {
        if (unit.type != "detector") continue;

        float cx = (unit.col + 0.5f) * m_cellSize;
        float cy = (unit.row + 0.5f) * m_cellSize;
        float pixelRadius = static_cast<float>(radius) * m_cellSize;

        // Draw filled radius circle
        sf::CircleShape circle(pixelRadius);
        circle.setOrigin(sf::Vector2f(pixelRadius, pixelRadius));
        circle.setPosition(sf::Vector2f(cx, cy));
        circle.setFillColor(DETECTOR_RADIUS_COLOR);
        circle.setOutlineColor(sf::Color(255, 180, 0, 100));
        circle.setOutlineThickness(1.5f);
        circle.setPointCount(48);  // smooth circle
        window.draw(circle);
    }
}

void MapVisualizer::drawUnits(sf::RenderWindow& window, sf::Font* font) const {
    float half = m_cellSize * 0.45f;
    if (half < 2.5f) half = 2.5f;

    for (const auto& unit : m_config.getUnits()) {
        float cx = (unit.col + 0.5f) * m_cellSize;
        float cy = (unit.row + 0.5f) * m_cellSize;

        if (unit.type == "seeker") {
            // ── Seekers: red triangle (points up) ──
            sf::CircleShape tri(half, 3);  // 3 sides = triangle
            tri.setOrigin(sf::Vector2f(half, half));
            tri.setPosition(sf::Vector2f(cx, cy));
            tri.setFillColor(SEEKER_COLOR);
            tri.setOutlineColor(sf::Color::White);
            tri.setOutlineThickness(1.5f);
            window.draw(tri);
        } else if (unit.type == "target") {
            // ── Targets: blue square ──
            float side = half * 1.6f;
            sf::RectangleShape sq(sf::Vector2f(side, side));
            sq.setOrigin(sf::Vector2f(side / 2.f, side / 2.f));
            sq.setPosition(sf::Vector2f(cx, cy));
            sq.setFillColor(TARGET_COLOR);
            sq.setOutlineColor(sf::Color::White);
            sq.setOutlineThickness(1.5f);
            window.draw(sq);
        } else if (unit.type == "detector") {
            // ── Detectors: orange diamond (rotated square) ──
            float side = half * 1.4f;
            sf::RectangleShape diamond(sf::Vector2f(side, side));
            diamond.setOrigin(sf::Vector2f(side / 2.f, side / 2.f));
            diamond.setPosition(sf::Vector2f(cx, cy));
            diamond.setRotation(sf::degrees(45.f));
            diamond.setFillColor(DETECTOR_COLOR);
            diamond.setOutlineColor(sf::Color::White);
            diamond.setOutlineThickness(1.5f);
            window.draw(diamond);
        }

        // ── Draw label letter if font available and cells are big enough ──
        if (font != nullptr && m_cellSize >= 10.0f) {
            std::string label;
            if (unit.type == "seeker")        label = "S";
            else if (unit.type == "target")   label = "T";
            else if (unit.type == "detector") label = "D";

            unsigned int fontSize = static_cast<unsigned int>(m_cellSize * 0.45f);
            if (fontSize < 8) fontSize = 8;

            sf::Text text(*font, label, fontSize);
            text.setFillColor(sf::Color::White);
            text.setStyle(sf::Text::Bold);

            // Center the letter on the cell
            sf::FloatRect bounds = text.getLocalBounds();
            text.setOrigin(sf::Vector2f(
                bounds.position.x + bounds.size.x / 2.f,
                bounds.position.y + bounds.size.y / 2.f));
            text.setPosition(sf::Vector2f(cx, cy));
            window.draw(text);
        }
    }
}

void MapVisualizer::drawHover(sf::RenderWindow& window, int hoverRow, int hoverCol) const {
    if (hoverRow < 0) return;

    sf::RectangleShape highlight(sf::Vector2f(m_cellSize, m_cellSize));
    highlight.setPosition(sf::Vector2f(hoverCol * m_cellSize, hoverRow * m_cellSize));
    highlight.setFillColor(HOVER_COLOR);
    highlight.setOutlineColor(sf::Color::White);
    highlight.setOutlineThickness(1.0f);
    window.draw(highlight);

    // If in detector mode, also show the radius preview on hover
    if (m_currentType == "detector" && hoverRow >= 0) {
        float cx = (hoverCol + 0.5f) * m_cellSize;
        float cy = (hoverRow + 0.5f) * m_cellSize;
        float pixelRadius = static_cast<float>(m_config.getDetectorRadius()) * m_cellSize;

        sf::CircleShape preview(pixelRadius);
        preview.setOrigin(sf::Vector2f(pixelRadius, pixelRadius));
        preview.setPosition(sf::Vector2f(cx, cy));
        preview.setFillColor(sf::Color(255, 180, 0, 20));
        preview.setOutlineColor(sf::Color(255, 180, 0, 120));
        preview.setOutlineThickness(1.0f);
        preview.setPointCount(48);
        window.draw(preview);
    }
}

void MapVisualizer::drawStatusBar(sf::RenderWindow& window, sf::Font* font) const {
    // Background panel
    sf::RectangleShape panel(sf::Vector2f(
        static_cast<float>(m_windowSize),
        static_cast<float>(m_panelHeight)));
    panel.setPosition(sf::Vector2f(0.f, static_cast<float>(m_windowSize)));
    panel.setFillColor(PANEL_COLOR);
    window.draw(panel);

    if (font == nullptr) return;

    // Status text
    std::ostringstream ss;
    std::string modeLabel;
    if (m_currentType == "seeker")        modeLabel = "SEEKER (S)";
    else if (m_currentType == "target")   modeLabel = "TARGET (T)";
    else if (m_currentType == "detector") modeLabel = "DETECTOR (D)";

    ss << "Mode: " << modeLabel
       << "  |  S:" << m_config.countType("seeker")
       << "  T:" << m_config.countType("target")
       << "  D:" << m_config.countType("detector")
       << "  R:" << m_config.getDetectorRadius()
       << "  |  LClick=Place  RClick=Remove  +/-=Radius  Enter=Save  Esc=Quit";

    // SFML 3: Text constructor is (font, string, characterSize)
    sf::Text text(*font, ss.str(), 13);
    text.setPosition(sf::Vector2f(8.f, static_cast<float>(m_windowSize) + 10.f));

    if (m_currentType == "seeker") {
        text.setFillColor(sf::Color(255, 120, 120));
    } else if (m_currentType == "target") {
        text.setFillColor(sf::Color(120, 160, 255));
    } else {
        text.setFillColor(sf::Color(255, 200, 80));
    }
    window.draw(text);
}

void MapVisualizer::updateTitle(sf::RenderWindow& window) const {
    std::ostringstream ss;
    ss << "UUV Spawn Tool  |  Mode: " << m_currentType
       << "  |  S:" << m_config.countType("seeker")
       << "  T:" << m_config.countType("target")
       << "  D:" << m_config.countType("detector")
       << "  R:" << m_config.getDetectorRadius();
    window.setTitle(ss.str());
}

// ════════════════════════════════════════════════════════════════════════════════
//  MAIN LOOP (SFML 3 event system)
// ════════════════════════════════════════════════════════════════════════════════

SpawnConfig MapVisualizer::run(const std::string& savePath) {
    // ── Create window ──
    unsigned int totalHeight = static_cast<unsigned int>(m_windowSize + m_panelHeight);
    unsigned int winW = static_cast<unsigned int>(m_windowSize);

    // SFML 3: VideoMode takes Vector2u
    sf::RenderWindow window(
        sf::VideoMode(sf::Vector2u(winW, totalHeight)),
        "UUV Spawn Tool",
        sf::Style::Close
    );
    window.setFramerateLimit(30);

    // ── Load a font ──
    sf::Font font;
    sf::Font* fontPtr = nullptr;
    const std::string fontPaths[] = {
        "/System/Library/Fonts/Menlo.ttc",
        "/System/Library/Fonts/SFNSMono.ttf",
        "/System/Library/Fonts/Helvetica.ttc",
        "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
        "/usr/share/fonts/TTF/DejaVuSansMono.ttf",
        "C:\\Windows\\Fonts\\consola.ttf",
    };
    // SFML 3: loadFromFile → openFromFile
    for (const auto& path : fontPaths) {
        if (font.openFromFile(path)) {
            fontPtr = &font;
            break;
        }
    }
    if (fontPtr == nullptr) {
        std::cout << "Warning: Could not load any system font.\n"
                  << "Status bar text will not appear.\n"
                  << "Controls: S=Seeker, T=Target, D=Detector, "
                  << "+/-=Radius, LClick=Place, RClick=Remove, Enter=Save, Esc=Quit\n";
    }

    // ── Hover tracking ──
    int hoverRow = -1, hoverCol = -1;

    // ── Event loop (SFML 3 style) ──
    while (window.isOpen()) {
        // SFML 3: pollEvent returns std::optional<sf::Event>
        while (auto event = window.pollEvent()) {
            // ── Window close ──
            if (event->is<sf::Event::Closed>()) {
                window.close();
                return SpawnConfig();
            }

            // ── Keyboard ──
            if (const auto* keyEvt = event->getIf<sf::Event::KeyPressed>()) {
                if (keyEvt->code == sf::Keyboard::Key::S) {
                    m_currentType = "seeker";
                    updateTitle(window);
                }
                else if (keyEvt->code == sf::Keyboard::Key::T) {
                    m_currentType = "target";
                    updateTitle(window);
                }
                else if (keyEvt->code == sf::Keyboard::Key::D) {
                    m_currentType = "detector";
                    updateTitle(window);
                }
                else if (keyEvt->code == sf::Keyboard::Key::C) {
                    m_config.clear();
                    updateTitle(window);
                }
                // ── Radius adjustment: + to increase, - to decrease ──
                else if (keyEvt->code == sf::Keyboard::Key::Equal ||    // + / =
                         keyEvt->code == sf::Keyboard::Key::Add) {      // numpad +
                    double r = m_config.getDetectorRadius();
                    m_config.setDetectorRadius(r + 0.5);
                    std::cout << "Detector radius: " << m_config.getDetectorRadius() << "\n";
                    updateTitle(window);
                }
                else if (keyEvt->code == sf::Keyboard::Key::Hyphen ||   // -
                         keyEvt->code == sf::Keyboard::Key::Subtract) {  // numpad -
                    double r = m_config.getDetectorRadius();
                    if (r > 0.5) {
                        m_config.setDetectorRadius(r - 0.5);
                    }
                    std::cout << "Detector radius: " << m_config.getDetectorRadius() << "\n";
                    updateTitle(window);
                }
                else if (keyEvt->code == sf::Keyboard::Key::Enter) {
                    if (m_config.totalUnits() > 0) {
                        if (!savePath.empty()) {
                            m_config.saveJSON(savePath);
                        }
                        m_config.printSummary();
                    } else {
                        std::cout << "No units placed. Nothing saved.\n";
                    }
                    window.close();
                    return m_config;
                }
                else if (keyEvt->code == sf::Keyboard::Key::Escape) {
                    window.close();
                    return SpawnConfig();
                }
            }

            // ── Mouse movement (hover) ──
            if (const auto* moveEvt = event->getIf<sf::Event::MouseMoved>()) {
                mouseToGrid(moveEvt->position.x, moveEvt->position.y,
                            hoverRow, hoverCol);
            }

            // ── Mouse click ──
            if (const auto* btnEvt = event->getIf<sf::Event::MouseButtonPressed>()) {
                int clickRow, clickCol;
                if (!mouseToGrid(btnEvt->position.x, btnEvt->position.y,
                                 clickRow, clickCol)) {
                    continue;
                }

                if (btnEvt->button == sf::Mouse::Button::Left) {
                    if (m_map.isWater(clickRow, clickCol)) {
                        if (!m_config.addUnit(m_currentType, clickRow, clickCol)) {
                            m_config.removeUnit(clickRow, clickCol);
                            m_config.addUnit(m_currentType, clickRow, clickCol);
                        }
                    }
                    updateTitle(window);
                }
                else if (btnEvt->button == sf::Mouse::Button::Right) {
                    m_config.removeUnit(clickRow, clickCol);
                    updateTitle(window);
                }
            }
        }

        // ── Draw ──
        window.clear(sf::Color::Black);
        drawGrid(window);
        drawDetectorRadii(window);   // draw radius circles under units
        drawUnits(window, fontPtr);
        drawHover(window, hoverRow, hoverCol);
        drawStatusBar(window, fontPtr);
        window.display();
    }

    return m_config;
}