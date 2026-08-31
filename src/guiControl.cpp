#include "guiControl.h"

#include <SFML/Graphics.hpp>
#include <TGUI/TGUI.hpp>
#include <TGUI/Backend/SFML-Graphics.hpp>
#include <TGUI/Widgets/FileDialog.hpp>

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void setSelection(GuiControlState& state, GuiAgentCategory category,
                  GuiAgentType type) {
    state.category.store(category);
    state.type.store(type);
}

void setSelectionFromName(GuiControlState& state, const std::string& selected) {
    if (selected == "Seeker / Basic") setSelection(state, GuiAgentCategory::Seeker, GuiAgentType::Basic);
    else if (selected == "Seeker / Fast") setSelection(state, GuiAgentCategory::Seeker, GuiAgentType::Fast);
    else if (selected == "Seeker / Evader") setSelection(state, GuiAgentCategory::Seeker, GuiAgentType::Evader);
    else if (selected == "Target / Basic") setSelection(state, GuiAgentCategory::Target, GuiAgentType::Basic);
    else if (selected == "Detector / Basic") setSelection(state, GuiAgentCategory::Detector, GuiAgentType::Basic);
    else if (selected == "Detector / Medium") setSelection(state, GuiAgentCategory::Detector, GuiAgentType::Medium);
    else if (selected == "Detector / Advanced") setSelection(state, GuiAgentCategory::Detector, GuiAgentType::Advanced);
    else if (selected == "Interceptor / Basic") setSelection(state, GuiAgentCategory::Interceptor, GuiAgentType::Basic);
    else if (selected == "Interceptor / Medium") setSelection(state, GuiAgentCategory::Interceptor, GuiAgentType::Medium);
    else if (selected == "Interceptor / Advanced") setSelection(state, GuiAgentCategory::Interceptor, GuiAgentType::Advanced);
}

}

void runGuiControlPanel(GuiControlState& state) {
    sf::RenderWindow window(
        sf::VideoMode(sf::Vector2u(380, 590)),
        "UUV Simulation Control",
        sf::Style::Close);
    window.setFramerateLimit(60);

    tgui::Gui gui(window);
    auto agentType = tgui::ComboBox::create();
    agentType->setPosition(35, 35);
    agentType->setSize(310, 34);
    agentType->addItem("Seeker / Basic");
    agentType->addItem("Seeker / Fast");
    agentType->addItem("Seeker / Evader");
    agentType->addItem("Target / Basic");
    agentType->addItem("Detector / Basic");
    agentType->addItem("Detector / Medium");
    agentType->addItem("Detector / Advanced");
    agentType->addItem("Interceptor / Basic");
    agentType->addItem("Interceptor / Medium");
    agentType->addItem("Interceptor / Advanced");
    agentType->setSelectedItem("Seeker / Basic");
    gui.add(agentType);

    auto addButton = tgui::Button::create("Select agent type");
    addButton->setPosition(35, 85);
    addButton->setSize(310, 42);
    gui.add(addButton);

    auto startButton = tgui::Button::create("Start simulation");
    startButton->setPosition(35, 145);
    startButton->setSize(145, 42);
    gui.add(startButton);

    auto resetButton = tgui::Button::create("Reset placements");
    resetButton->setPosition(200, 145);
    resetButton->setSize(145, 42);
    gui.add(resetButton);

    auto info = tgui::Label::create(
        "Choose a type, then left-click a water cell\n"
        "in the simulation window to place it.");
    info->setPosition(35, 215);
    gui.add(info);

    auto applySelection = [&]() {
        const std::string selected = agentType->getSelectedItem().toStdString();
        if (selected == "Seeker / Basic") setSelection(state, GuiAgentCategory::Seeker, GuiAgentType::Basic);
        else if (selected == "Seeker / Fast") setSelection(state, GuiAgentCategory::Seeker, GuiAgentType::Fast);
        else if (selected == "Seeker / Evader") setSelection(state, GuiAgentCategory::Seeker, GuiAgentType::Evader);
        else if (selected == "Target / Basic") setSelection(state, GuiAgentCategory::Target, GuiAgentType::Basic);
        else if (selected == "Detector / Basic") setSelection(state, GuiAgentCategory::Detector, GuiAgentType::Basic);
        else if (selected == "Detector / Medium") setSelection(state, GuiAgentCategory::Detector, GuiAgentType::Medium);
        else if (selected == "Detector / Advanced") setSelection(state, GuiAgentCategory::Detector, GuiAgentType::Advanced);
        else if (selected == "Interceptor / Basic") setSelection(state, GuiAgentCategory::Interceptor, GuiAgentType::Basic);
        else if (selected == "Interceptor / Medium") setSelection(state, GuiAgentCategory::Interceptor, GuiAgentType::Medium);
        else if (selected == "Interceptor / Advanced") setSelection(state, GuiAgentCategory::Interceptor, GuiAgentType::Advanced);
    };

    addButton->onPress(applySelection);
    applySelection();
    startButton->onPress([&state]() { state.startRequested.store(true); });
    resetButton->onPress([&state]() { state.resetRequested.store(true); });

    while (window.isOpen() && !state.exitRequested.load()) {
        while (auto event = window.pollEvent()) {
            gui.handleEvent(*event);
            if (event->is<sf::Event::Closed>()) {
                state.panelClosed.store(true);
                state.exitRequested.store(true);
                window.close();
            }
        }

        window.clear(sf::Color(30, 30, 30));
        gui.draw();
        window.display();
    }

    state.panelClosed.store(true);
}

struct GuiControlPanel::Impl {
    GuiControlState& state;
    tgui::Gui gui;
    tgui::ComboBox::Ptr agentType;
    tgui::ListView::Ptr agentTable;

        Impl(sf::RenderWindow& window, GuiControlState& controlState)
        : state(controlState),
          gui(window), agentType(tgui::ComboBox::create()),
          agentTable(tgui::ListView::create()) {
                auto panel = tgui::Panel::create();
                panel->setPosition(static_cast<float>(window.getSize().x - 380u), 0);
                panel->setSize(380, static_cast<float>(window.getSize().y));
                panel->getRenderer()->setBackgroundColor(tgui::Color(30, 30, 30));
                gui.add(panel);

        agentType->setPosition(35, 35);
        agentType->setSize(310, 34);
        const std::string options[] = {
            "Seeker / Basic", "Seeker / Fast", "Seeker / Evader",
            "Target / Basic", "Detector / Basic", "Detector / Medium",
            "Detector / Advanced", "Interceptor / Basic",
            "Interceptor / Medium", "Interceptor / Advanced"
        };
        for (const auto& option : options) agentType->addItem(option);
        agentType->setSelectedItem(options[0]);
        panel->add(agentType);

        auto modelTitle = tgui::Label::create("Model Selection");
        modelTitle->setPosition(35, 265);
        modelTitle->setTextSize(18);
        panel->add(modelTitle);
        agentTable->setPosition(35, 300);
        agentTable->setSize(310, 120);
        agentTable->addColumn("Name", 90);
        agentTable->addColumn("Type", 165);
        agentTable->addColumn("#", 40);
        panel->add(agentTable);

        auto runTitle = tgui::Label::create("Simulation Run");
        runTitle->setPosition(35, 450);
        runTitle->setTextSize(18);
        panel->add(runTitle);

        auto iterationsLabel = tgui::Label::create("Iterations");
        iterationsLabel->setPosition(35, 482);
        panel->add(iterationsLabel);
        auto iterationsInput = tgui::EditBox::create();
        iterationsInput->setPosition(210, 475);
        iterationsInput->setSize(135, 30);
        iterationsInput->setText(std::to_string(state.iterationCount.load()));
        panel->add(iterationsInput);

        auto startingNoiseLabel = tgui::Label::create("Starting noise");
        startingNoiseLabel->setPosition(35, 522);
        panel->add(startingNoiseLabel);
        auto startingNoiseInput = tgui::EditBox::create();
        startingNoiseInput->setPosition(210, 515);
        startingNoiseInput->setSize(135, 30);
        startingNoiseInput->setText(std::to_string(state.startingNoise.load()));
        panel->add(startingNoiseInput);

        auto noiseIncrementLabel = tgui::Label::create("Noise increment");
        noiseIncrementLabel->setPosition(35, 562);
        panel->add(noiseIncrementLabel);
        auto noiseIncrementInput = tgui::EditBox::create();
        noiseIncrementInput->setPosition(210, 555);
        noiseIncrementInput->setSize(135, 30);
        noiseIncrementInput->setText(std::to_string(state.noiseIncrement.load()));
        panel->add(noiseIncrementInput);

        auto validationMessage = tgui::Label::create();
        validationMessage->setPosition(35, 600);
        validationMessage->getRenderer()->setTextColor(tgui::Color(255, 120, 120));
        panel->add(validationMessage);

        auto selectButton = tgui::Button::create("Select agent type");
        selectButton->setPosition(35, 85);
        selectButton->setSize(310, 42);
        panel->add(selectButton);
        auto selectFileButton = tgui::Button::create("Select file");
        selectFileButton->setPosition(35, 145);
        selectFileButton->setSize(145, 42);
        panel->add(selectFileButton);
        auto resetButton = tgui::Button::create("Reset placements");
        resetButton->setPosition(200, 145);
        resetButton->setSize(145, 42);
        panel->add(resetButton);

        auto startButton = tgui::Button::create("Start simulation");
        startButton->setPosition(35, 645);
        startButton->setSize(310, 42);
        panel->add(startButton);

        auto exitButton = tgui::Button::create("Exit");
        exitButton->setPosition(35, 695);
        exitButton->setSize(310, 35);
        panel->add(exitButton);
        auto info = tgui::Label::create("Choose a type, then left-click a water cell\nin the map to place it.");
        info->setPosition(35, 215);
        panel->add(info);

        auto applySelection = [this]() {
            const std::string selected = agentType->getSelectedItem().toStdString();
            const auto set = [this](GuiAgentCategory category, GuiAgentType type) {
                state.category.store(category);
                state.type.store(type);
            };
            if (selected == "Seeker / Basic") set(GuiAgentCategory::Seeker, GuiAgentType::Basic);
            else if (selected == "Seeker / Fast") set(GuiAgentCategory::Seeker, GuiAgentType::Fast);
            else if (selected == "Seeker / Evader") set(GuiAgentCategory::Seeker, GuiAgentType::Evader);
            else if (selected == "Target / Basic") set(GuiAgentCategory::Target, GuiAgentType::Basic);
            else if (selected == "Detector / Basic") set(GuiAgentCategory::Detector, GuiAgentType::Basic);
            else if (selected == "Detector / Medium") set(GuiAgentCategory::Detector, GuiAgentType::Medium);
            else if (selected == "Detector / Advanced") set(GuiAgentCategory::Detector, GuiAgentType::Advanced);
            else if (selected == "Interceptor / Basic") set(GuiAgentCategory::Interceptor, GuiAgentType::Basic);
            else if (selected == "Interceptor / Medium") set(GuiAgentCategory::Interceptor, GuiAgentType::Medium);
            else if (selected == "Interceptor / Advanced") set(GuiAgentCategory::Interceptor, GuiAgentType::Advanced);
        };
        selectButton->onPress(applySelection);
        selectFileButton->onPress([this]() {
            auto fileDialog = tgui::FileDialog::create("Select map shapefile", "Open");
            fileDialog->setSize(620, 440);
            fileDialog->setFileTypeFilters({
                {"Shapefiles", {"*.shp"}},
                {"All files", {}}
            });
            fileDialog->onFileSelect([this, fileDialog](const std::vector<tgui::Filesystem::Path>& paths) {
                if (paths.empty()) {
                    return;
                }

                state.selectedMapPath = paths.front().asString().toStdString();
                state.mapReloadRequested.store(true);
            });
            gui.add(fileDialog);
        });
        agentType->onItemSelect([this](const tgui::String& selected) {
            setSelectionFromName(state, selected.toStdString());
        });
        applySelection();
        startButton->onPress([this, iterationsInput, startingNoiseInput,
                              noiseIncrementInput, validationMessage]() {
            try {
                const int iterations = std::stoi(iterationsInput->getText().toStdString());
                const double startingNoise = std::stod(startingNoiseInput->getText().toStdString());
                const double noiseIncrement = std::stod(noiseIncrementInput->getText().toStdString());

                if (iterations < 1 || startingNoise < 0.0 || noiseIncrement < 0.0 ||
                    !std::isfinite(startingNoise) || !std::isfinite(noiseIncrement)) {
                    throw std::invalid_argument("invalid run settings");
                }

                state.iterationCount.store(iterations);
                state.startingNoise.store(startingNoise);
                state.noiseIncrement.store(noiseIncrement);
                validationMessage->setText("");
                state.startRequested.store(true);
            }
            catch (const std::exception&) {
                validationMessage->setText("Enter valid non-negative numbers.");
            }
        });
        resetButton->onPress([this]() { state.resetRequested.store(true); });
        exitButton->onPress([this]() {
            state.exitRequested.store(true);
        });
    }
};

GuiControlPanel::GuiControlPanel(sf::RenderWindow& window, GuiControlState& state)
    : m_impl(std::make_unique<Impl>(window, state)) {}

GuiControlPanel::~GuiControlPanel() = default;

bool GuiControlPanel::handleEvent(const sf::Event& event) {
    return m_impl->gui.handleEvent(event);
}

void GuiControlPanel::draw() {
    m_impl->gui.draw();
}

void GuiControlPanel::setAgents(const std::vector<UnitSpawn>& agents) {
    struct AgentGroup {
        std::string type;
        int quantity;
    };

    std::vector<AgentGroup> groups;
    for (const auto& agent : agents) {
        const std::string type = agent.category + " / " + agent.type;
        auto existing = std::find_if(
            groups.begin(), groups.end(),
            [&type](const AgentGroup& group) { return group.type == type; });
        if (existing == groups.end()) {
            groups.push_back({type, 1});
        } else {
            ++existing->quantity;
        }
    }

    m_impl->agentTable->removeAllItems();
    int number = 1;
    for (const auto& group : groups) {
        m_impl->agentTable->addItem({
            "Agent_" + std::to_string(number++),
            group.type,
            std::to_string(group.quantity)
        });
    }
}

