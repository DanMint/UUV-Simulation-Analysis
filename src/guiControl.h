#ifndef GUI_CONTROL_H
#define GUI_CONTROL_H

#include <atomic>
#include <memory>
#include <vector>

#include <SFML/Graphics.hpp>

#include "spawnConfig.h"

enum class GuiAgentCategory {
    None,
    Seeker,
    Target,
    Detector,
    Interceptor
};

enum class GuiAgentType {
    None,
    Basic,
    Fast,
    Evader,
    Medium,
    Advanced
};

struct GuiControlState {
    std::atomic<GuiAgentCategory> category{GuiAgentCategory::None};
    std::atomic<GuiAgentType> type{GuiAgentType::None};
    std::atomic<int> iterationCount{1};
    std::atomic<double> startingNoise{0.0};
    std::atomic<double> noiseIncrement{0.0};
    std::string selectedMapPath;
    std::atomic<bool> mapReloadRequested{false};
    std::atomic<bool> startRequested{false};
    std::atomic<bool> resetRequested{false};
    std::atomic<bool> exitRequested{false};
    std::atomic<bool> panelClosed{false};
};

class GuiControlPanel {
public:
    GuiControlPanel(sf::RenderWindow& window, GuiControlState& state);
    ~GuiControlPanel();

    GuiControlPanel(const GuiControlPanel&) = delete;
    GuiControlPanel& operator=(const GuiControlPanel&) = delete;

    bool handleEvent(const sf::Event& event);
    void draw();
    void setAgents(const std::vector<UnitSpawn>& agents);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

#endif