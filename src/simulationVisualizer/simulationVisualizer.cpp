#include "simulationVisualizer.h"
#include <iostream>
#include <sstream>
#include <cmath>
#include <algorithm>
#include <cstdint>

// ════════════════════════════════════════════════════════════════════════════════
//  COLOR CONSTANTS
// ════════════════════════════════════════════════════════════════════════════════

const sf::Color SimulationVisualizer::WATER_COLOR             = sf::Color(20,  50,  120);
const sf::Color SimulationVisualizer::LAND_COLOR              = sf::Color(50,  110, 50);
const sf::Color SimulationVisualizer::SEEKER_COLOR            = sf::Color(220, 30,  30);
const sf::Color SimulationVisualizer::TARGET_COLOR            = sf::Color(30,  100, 220);
const sf::Color SimulationVisualizer::DETECTOR_COLOR          = sf::Color(255, 180, 0);
const sf::Color SimulationVisualizer::DETECTOR_RADIUS_COLOR   = sf::Color(255, 180, 0, 40);
const sf::Color SimulationVisualizer::INTERCEPTOR_COLOR       = sf::Color(160, 80,  255);
const sf::Color SimulationVisualizer::INTERCEPTOR_RADIUS_COLOR= sf::Color(160, 80,  255, 40);
const sf::Color SimulationVisualizer::TRAIL_FADE              = sf::Color(200, 200, 200, 100);
const sf::Color SimulationVisualizer::GRID_LINE_COLOR         = sf::Color(40,  40,  40,  60);
const sf::Color SimulationVisualizer::PANEL_COLOR             = sf::Color(30,  30,  30);
const sf::Color SimulationVisualizer::BG_COLOR                = sf::Color(10,  10,  10);
const sf::Color SimulationVisualizer::LEGEND_BG_COLOR         = sf::Color(0,   0,   0,  180);

sf::Color SimulationVisualizer::attackerColor(const std::string& vt) {
    if (vt == "bluerov2")    return sf::Color(255, 140, 60);
    if (vt == "riptide")     return sf::Color(210, 255, 60);
    if (vt == "blueboat")    return sf::Color(60, 220, 130);
    if (vt == "yuco")        return sf::Color(0, 220, 200);
    if (vt == "nemosens")    return sf::Color(255, 80, 200);
    if (vt == "hugin")       return sf::Color(140, 70, 20);
    if (vt == "tb2")         return sf::Color(0, 255, 255);
    if (vt == "queenhornet") return sf::Color(200, 180, 255);
    if (vt == "shahed")      return sf::Color(255, 190, 220);
    return sf::Color(255, 140, 60);
}

// ════════════════════════════════════════════════════════════════════════════════
//  CONSTRUCTOR — starts at very slow speed (600ms) for live demos.
//  Use [+] to speed up, [-] to slow down, [Space] to pause.
// ════════════════════════════════════════════════════════════════════════════════

SimulationVisualizer::SimulationVisualizer(const MapCreation& map, Simulation& sim, int windowSize)
    : m_map(map), m_sim(sim),
      m_windowSize(windowSize),
      m_panelHeight(44),
      m_paused(false),
      m_delayMs(600),
      m_finished(false),
      m_showLegend(false)
{
    m_cellSize = static_cast<float>(m_windowSize) / m_map.getCellsN();
    m_seekerTrails.resize(m_sim.getSeekers().size());
    m_attackerTrails.resize(m_sim.getAttackers().size());
}

// ════════════════════════════════════════════════════════════════════════════════
//  COORDINATE CONVERSION
// ════════════════════════════════════════════════════════════════════════════════

bool SimulationVisualizer::mouseToGrid(int mouseX, int mouseY,
                                        int& outRow, int& outCol) const {
    if (mouseX < 0 || mouseX >= m_windowSize ||
        mouseY < 0 || mouseY >= m_windowSize)
        return false;
    outCol = static_cast<int>(mouseX / m_cellSize);
    outRow = static_cast<int>(mouseY / m_cellSize);
    int n = m_map.getCellsN();
    return (outRow >= 0 && outRow < n && outCol >= 0 && outCol < n);
}

// ════════════════════════════════════════════════════════════════════════════════
//  DRAW GRID
// ════════════════════════════════════════════════════════════════════════════════

void SimulationVisualizer::drawGrid(sf::RenderWindow& window) const {
    int n = m_map.getCellsN();
    const auto& grid = m_map.getGrid();
    sf::RectangleShape cell(sf::Vector2f(m_cellSize, m_cellSize));
    cell.setOutlineThickness(0.5f);
    cell.setOutlineColor(GRID_LINE_COLOR);

    for (int row = 0; row < n; row++) {
        for (int col = 0; col < n; col++) {
            cell.setPosition(sf::Vector2f(col * m_cellSize, row * m_cellSize));
            cell.setFillColor(grid[row][col] == 0 ? WATER_COLOR : LAND_COLOR);
            window.draw(cell);
        }
    }
}

// ════════════════════════════════════════════════════════════════════════════════
//  DRAW TRAILS
// ════════════════════════════════════════════════════════════════════════════════

void SimulationVisualizer::drawTrails(sf::RenderWindow& window) const {
    auto drawTrailSet = [&](const std::vector<std::vector<TrailPoint>>& trails,
                            sf::Color baseColor) {
        for (const auto& trail : trails) {
            if (trail.size() < 2) continue;
            int total = static_cast<int>(trail.size());
            int count = std::min(total, MAX_TRAIL_POINTS);
            int start = total - count;
            sf::VertexArray lines(sf::PrimitiveType::LineStrip, count);
            for (int i = 0; i < count; i++) {
                int idx = start + i;
                float alpha = 50 + (155.0f * i / std::max(count, 1));
                lines[i].position = sf::Vector2f(
                    (trail[idx].col + 0.5f) * m_cellSize,
                    (trail[idx].row + 0.5f) * m_cellSize);
                lines[i].color = sf::Color(baseColor.r, baseColor.g, baseColor.b,
                                           static_cast<std::uint8_t>(alpha));
            }
            window.draw(lines);
        }
    };

    drawTrailSet(m_seekerTrails, sf::Color(220, 30, 30));
    drawTrailSet(m_attackerTrails, sf::Color(255, 140, 60));
}

// ════════════════════════════════════════════════════════════════════════════════
//  DRAW AGENTS
// ════════════════════════════════════════════════════════════════════════════════

void SimulationVisualizer::drawAgents(sf::RenderWindow& window, sf::Font* font) const {
    float half = m_cellSize * 0.4f;

    for (const auto& s : m_sim.getSeekers()) {
        if (!s.alive && !s.reachedTarget) continue;
        float cx = (s.col + 0.5f) * m_cellSize;
        float cy = (s.row + 0.5f) * m_cellSize;

        sf::CircleShape circle(half);
        circle.setFillColor(s.detected ? sf::Color(255, 100, 100) : SEEKER_COLOR);
        circle.setOutlineThickness(2.f);
        circle.setOutlineColor(s.reachedTarget ? sf::Color::Green : sf::Color::White);
        circle.setPosition(sf::Vector2f(cx - half, cy - half));
        window.draw(circle);

        if (font) {
            sf::Text txt(*font, std::to_string(s.id), static_cast<unsigned>(half));
            txt.setFillColor(sf::Color::White);
            txt.setPosition(sf::Vector2f(cx - half * 0.5f, cy - half * 0.5f));
            window.draw(txt);
        }
    }

    for (const auto& t : m_sim.getTargets()) {
        if (!t.alive) continue;
        float cx = (t.col + 0.5f) * m_cellSize;
        float cy = (t.row + 0.5f) * m_cellSize;

        sf::RectangleShape rect(sf::Vector2f(half * 1.6f, half * 1.6f));
        rect.setFillColor(TARGET_COLOR);
        rect.setOutlineThickness(2.f);
        rect.setOutlineColor(sf::Color::White);
        rect.setOrigin(sf::Vector2f(half * 0.8f, half * 0.8f));
        rect.setPosition(sf::Vector2f(cx, cy));
        window.draw(rect);
    }

    for (const auto& d : m_sim.getDetectors()) {
        if (!d.alive) continue;
        float cx = (d.col + 0.5f) * m_cellSize;
        float cy = (d.row + 0.5f) * m_cellSize;

        sf::CircleShape circle(half * 0.7f);
        circle.setFillColor(DETECTOR_COLOR);
        circle.setOutlineThickness(1.f);
        circle.setOutlineColor(sf::Color::Black);
        circle.setPosition(sf::Vector2f(cx - half * 0.7f, cy - half * 0.7f));
        window.draw(circle);
    }

    for (const auto& i : m_sim.getInterceptors()) {
        if (!i.alive) continue;
        float cx = (i.col + 0.5f) * m_cellSize;
        float cy = (i.row + 0.5f) * m_cellSize;

        sf::CircleShape circle(half * 0.7f);
        circle.setFillColor(INTERCEPTOR_COLOR);
        circle.setOutlineThickness(1.f);
        circle.setOutlineColor(sf::Color::Black);
        circle.setPosition(sf::Vector2f(cx - half * 0.7f, cy - half * 0.7f));
        window.draw(circle);
    }

    for (const auto& a : m_sim.getAttackers()) {
        if (!a.alive) continue;
        float cx = (a.col + 0.5f) * m_cellSize;
        float cy = (a.row + 0.5f) * m_cellSize;

        sf::CircleShape circle(half * 1.1f);
        circle.setFillColor(attackerColor(a.specs.agentType));
        circle.setOutlineThickness(a.detected ? 3.f : 1.f);
        circle.setOutlineColor(a.detected ? sf::Color::Red : sf::Color::Black);
        circle.setPosition(sf::Vector2f(cx - half * 1.1f, cy - half * 1.1f));
        window.draw(circle);

        if (font) {
            sf::Text txt(*font, std::to_string(a.id), static_cast<unsigned>(half));
            txt.setFillColor(sf::Color::White);
            txt.setPosition(sf::Vector2f(cx - half * 0.5f, cy - half * 0.5f));
            window.draw(txt);
        }
    }
}

// ════════════════════════════════════════════════════════════════════════════════
//  DRAW DETECTOR RADII
// ════════════════════════════════════════════════════════════════════════════════

void SimulationVisualizer::drawDetectorRadii(sf::RenderWindow& window) const {
    for (const auto& d : m_sim.getDetectors()) {
        if (!d.alive) continue;
        float cx = (d.col + 0.5f) * m_cellSize;
        float cy = (d.row + 0.5f) * m_cellSize;
        float radius = static_cast<float>(d.sensingRadius) * m_cellSize;

        sf::CircleShape circle(radius);
        circle.setFillColor(DETECTOR_RADIUS_COLOR);
        circle.setOutlineThickness(1.f);
        circle.setOutlineColor(sf::Color(255, 180, 0, 80));
        circle.setPosition(sf::Vector2f(cx - radius, cy - radius));
        window.draw(circle);
    }
}

// ════════════════════════════════════════════════════════════════════════════════
//  DRAW INTERCEPTOR RADII
// ════════════════════════════════════════════════════════════════════════════════

void SimulationVisualizer::drawInterceptorRadii(sf::RenderWindow& window) const {
    for (const auto& i : m_sim.getInterceptors()) {
        if (!i.alive) continue;
        float cx = (i.col + 0.5f) * m_cellSize;
        float cy = (i.row + 0.5f) * m_cellSize;
        float radius = static_cast<float>(i.killRadius) * m_cellSize;

        sf::CircleShape circle(radius);
        circle.setFillColor(INTERCEPTOR_RADIUS_COLOR);
        circle.setOutlineThickness(1.f);
        circle.setOutlineColor(sf::Color(160, 80, 255, 80));
        circle.setPosition(sf::Vector2f(cx - radius, cy - radius));
        window.draw(circle);
    }
}

// ════════════════════════════════════════════════════════════════════════════════
//  DRAW LEGEND
// ════════════════════════════════════════════════════════════════════════════════

void SimulationVisualizer::drawLegend(sf::RenderWindow& window, sf::Font* font) const {
    if (!font || !m_showLegend) return;

    sf::RectangleShape bg(sf::Vector2f(220, 240));
    bg.setFillColor(LEGEND_BG_COLOR);
    bg.setPosition(sf::Vector2f(10, 10));
    window.draw(bg);

    int y = 20;
    auto entry = [&](const char* label, sf::Color color, bool isCircle) {
        sf::Text txt(*font, label, 14);
        txt.setFillColor(sf::Color::White);
        txt.setPosition(sf::Vector2f(38, static_cast<float>(y)));
        window.draw(txt);
        if (isCircle) {
            sf::CircleShape dot(6);
            dot.setFillColor(color);
            dot.setPosition(sf::Vector2f(20, static_cast<float>(y)));
            window.draw(dot);
        } else {
            sf::RectangleShape rect(sf::Vector2f(12, 12));
            rect.setFillColor(color);
            rect.setPosition(sf::Vector2f(20, static_cast<float>(y)));
            window.draw(rect);
        }
        y += 20;
    };

    sf::Text title(*font, "Legend", 16);
    title.setFillColor(sf::Color::White);
    title.setPosition(sf::Vector2f(20, static_cast<float>(y)));
    window.draw(title);
    y += 25;

    entry("Seeker (red)",          SEEKER_COLOR,      true);
    entry("Target (blue)",         TARGET_COLOR,      false);
    entry("Detector (orange)",     DETECTOR_COLOR,    true);
    entry("Interceptor (purple)",  INTERCEPTOR_COLOR, true);
    entry("Attacker (type color)", sf::Color(255, 140, 60), true);
    entry("Detect radius",         DETECTOR_RADIUS_COLOR, true);
    entry("Kill radius",           INTERCEPTOR_RADIUS_COLOR, true);

    sf::Text hint(*font, "L = toggle", 12);
    hint.setFillColor(sf::Color(180, 180, 180));
    hint.setPosition(sf::Vector2f(20, static_cast<float>(y + 5)));
    window.draw(hint);
}

// ════════════════════════════════════════════════════════════════════════════════
//  DRAW UI PANEL
// ════════════════════════════════════════════════════════════════════════════════

void SimulationVisualizer::drawUI(sf::RenderWindow& window, sf::Font* font,
                                    int step, int totalSteps) const {
    sf::RectangleShape panel(sf::Vector2f(static_cast<float>(m_windowSize),
                                           static_cast<float>(m_panelHeight)));
    panel.setFillColor(PANEL_COLOR);
    panel.setPosition(sf::Vector2f(0, static_cast<float>(m_windowSize)));
    window.draw(panel);

    if (!font) return;

    int aliveSeekers = 0, aliveTargets = 0, aliveAttackers = 0;
    for (const auto& s : m_sim.getSeekers()) { if (s.alive) aliveSeekers++; }
    for (const auto& t : m_sim.getTargets()) { if (t.alive) aliveTargets++; }
    for (const auto& a : m_sim.getAttackers()) { if (a.alive) aliveAttackers++; }

    std::ostringstream info;
    info << "Step " << step << "/" << totalSteps;
    info << "  |  S:" << aliveSeekers << "/" << m_sim.getSeekers().size();
    info << "  T:" << aliveTargets << "/" << m_sim.getTargets().size();
    info << "  A:" << aliveAttackers << "/" << m_sim.getAttackers().size();
    info << "  |  ";
    if (m_sim.isFinished()) {
        info << "FINISHED";
    } else if (m_paused) {
        info << "PAUSED";
    } else {
        info << "RUNNING";
    }
    info << "  |  " << (1000 / std::max(m_delayMs, 1)) << " step/s";
    info << "  |  [Space] Pause  [+/-] Speed  [Enter] Step  [L] Legend";

    sf::Text txt(*font, info.str(), 15);
    txt.setFillColor(sf::Color::White);
    txt.setPosition(sf::Vector2f(10, static_cast<float>(m_windowSize) + 13));
    window.draw(txt);

    // Draw a speed bar indicator
    float barWidth = 80.0f;
    float barHeight = 8.0f;
    float barX = static_cast<float>(m_windowSize) - barWidth - 120.0f;
    float barY = static_cast<float>(m_windowSize) + 18.0f;

    sf::RectangleShape speedBarBg(sf::Vector2f(barWidth, barHeight));
    speedBarBg.setFillColor(sf::Color(60, 60, 60));
    speedBarBg.setPosition(sf::Vector2f(barX, barY));
    window.draw(speedBarBg);

    float fillRatio = 1.0f - (static_cast<float>(m_delayMs) - 5.0f) / (2000.0f - 5.0f);
    fillRatio = std::max(0.0f, std::min(1.0f, fillRatio));
    sf::RectangleShape speedBarFill(sf::Vector2f(barWidth * fillRatio, barHeight));
    speedBarFill.setFillColor(fillRatio > 0.5f ? sf::Color(100, 200, 100) : sf::Color(200, 200, 80));
    speedBarFill.setPosition(sf::Vector2f(barX, barY));
    window.draw(speedBarFill);

    if (m_sim.isFinished()) {
        sf::Text closeHint(*font, "Esc to close", 14);
        closeHint.setFillColor(sf::Color(255, 200, 100));
        closeHint.setPosition(sf::Vector2f(static_cast<float>(m_windowSize) - 110,
                               static_cast<float>(m_windowSize) + 14));
        window.draw(closeHint);
    }
}

// ════════════════════════════════════════════════════════════════════════════════
//  MAIN LOOP — very slow default (600ms) so you can watch agents move.
//  Speed range: 5ms (fastest) to 2000ms (slowest), step size 10ms.
//  Title updates dynamically with [PAUSED] / [FINISHED] status.
// ════════════════════════════════════════════════════════════════════════════════

SimResult SimulationVisualizer::run() {
    std::string baseTitle = "UUV Simulation - Live Visualizer";

    sf::RenderWindow window(
        sf::VideoMode(sf::Vector2u(static_cast<unsigned int>(m_windowSize),
                                   static_cast<unsigned int>(m_windowSize + m_panelHeight))),
        baseTitle,
        sf::Style::Close);
    window.setFramerateLimit(60);

    // ── Font loading with many fallback paths for Windows ──
    sf::Font font;
    sf::Font* fontPtr = nullptr;
    const std::string fontPaths[] = {
        "C:/Windows/Fonts/arial.ttf",
        "C:/Windows/Fonts/segoeui.ttf",
        "C:/Windows/Fonts/consola.ttf",
        "C:/Windows/Fonts/calibri.ttf",
        "C:/Windows/Fonts/tahoma.ttf",
        "C:/Windows/Fonts/micross.ttf",
        "C:/Windows/Fonts/verdana.ttf",
        "C:/Windows/Fonts/DejaVuSans.ttf",
    };
    for (const auto& p : fontPaths) {
        if (font.openFromFile(p)) { fontPtr = &font; break; }
    }

    auto updateTrails = [&]() {
        auto updateOne = [&](auto& agents, auto& trails, bool includeReached) {
            trails.resize(agents.size());
            for (size_t i = 0; i < agents.size(); i++) {
                bool active = agents[i].alive || (includeReached && agents[i].reachedTarget);
                if (active) {
                    trails[i].push_back({agents[i].row, agents[i].col});
                    while (static_cast<int>(trails[i].size()) > MAX_TRAIL_POINTS)
                        trails[i].erase(trails[i].begin());
                }
            }
        };
        updateOne(m_sim.getSeekers(), m_seekerTrails, true);
        updateOne(m_sim.getAttackers(), m_attackerTrails, false);
    };

    updateTrails();

    while (window.isOpen()) {
        // ── Update window title with paused/finished status ──
        std::string title = baseTitle;
        if (m_paused) title += " [PAUSED]";
        if (m_sim.isFinished()) title += " [FINISHED]";
        window.setTitle(title);

        // ── Event handling ──
        while (auto event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                window.close();
                break;
            }
            if (const auto* k = event->getIf<sf::Event::KeyPressed>()) {
                switch (k->code) {
                    case sf::Keyboard::Key::Escape:
                        if (!m_sim.isFinished()) m_sim.finishFromCurrentState();
                        window.close();
                        break;
                    case sf::Keyboard::Key::Space:
                        m_paused = !m_paused;
                        break;
                    case sf::Keyboard::Key::Equal:
                    case sf::Keyboard::Key::Add:
                        m_delayMs = std::max(5, m_delayMs - 10);
                        break;
                    case sf::Keyboard::Key::Hyphen:
                    case sf::Keyboard::Key::Subtract:
                        m_delayMs = std::min(2000, m_delayMs + 10);
                        break;
                    case sf::Keyboard::Key::Enter:
                        if (m_paused && !m_sim.isFinished()) {
                            m_sim.stepOnce();
                            updateTrails();
                        }
                        break;
                    case sf::Keyboard::Key::L:
                        m_showLegend = !m_showLegend;
                        break;
                    default:
                        break;
                }
            }
        }

        // ── Advance simulation ──
        if (!m_paused && !m_sim.isFinished()) {
            m_sim.stepOnce();
            updateTrails();
        }

        // ── Render ──
        window.clear(BG_COLOR);
        drawGrid(window);
        drawDetectorRadii(window);
        drawInterceptorRadii(window);
        drawTrails(window);
        drawAgents(window, fontPtr);
        drawLegend(window, fontPtr);
        drawUI(window, fontPtr, m_sim.getStep(), m_sim.getMaxSteps());
        window.display();

        // ── Throttle ──
        if (!m_paused && m_delayMs > 0 && !m_sim.isFinished()) {
            sf::sleep(sf::milliseconds(m_delayMs));
        } else if (m_sim.isFinished()) {
            sf::sleep(sf::milliseconds(50));
        }
    }

    // ── Finish simulation if user closed early ──
    if (!m_sim.isFinished()) {
        m_sim.runFromCurrentState();
    }

    return m_sim.buildResult(m_sim.getStep());
}
