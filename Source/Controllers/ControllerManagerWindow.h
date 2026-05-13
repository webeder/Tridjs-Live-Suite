#pragma once

#include <JuceHeader.h>
#include <vector>
#include "../LanguageManager.h"

/**
 * ControllerManagerWindow
 * Premium UI to manage MIDI controllers and settings.
 */
class ControllerManagerWindow : public juce::DocumentWindow
{
public:
    class ContentComponent : public juce::Component, 
                             public juce::TextEditor::Listener,
                             public juce::ChangeListener
    {
    public:
        ContentComponent(const juce::File& mappingsDir, 
                        std::function<void(const juce::File&)> onActivate)
            : targetFolder(mappingsDir), onActivateCallback(onActivate)
        {
            // Sidebar Navigation
            setupNavButton(btnControladores, "", "settings_input_component", 0);
            setupNavButton(btnConfig, "", "settings", 1);
            setupNavButton(btnIdiomas, "", "language", 2);
            
            // Sidebar Footer Buttons
            addBtn.onClick = [this] { importMapping(); };
            addAndMakeVisible(addBtn);

            setupFooterLink(supportBtn, "", "help");
            setupFooterLink(firmwareBtn, "", "update");

            // Header - Search Bar
            searchBox.setTextToShowWhenEmpty("", juce::Colours::grey.withAlpha(0.4f));
            searchBox.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff0a0a0a));
            searchBox.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff333333));
            searchBox.setJustification(juce::Justification::centred);
            searchBox.addListener(this);
            addAndMakeVisible(searchBox);

            // Table Header
            tableHeader.addAndMakeVisible(new juce::Label("", ""));
            tableHeader.addAndMakeVisible(new juce::Label("", ""));
            tableHeader.addAndMakeVisible(new juce::Label("", ""));
            tableHeader.addAndMakeVisible(new juce::Label("", ""));
            addAndMakeVisible(tableHeader);

            viewport.setViewedComponent(&contentContainer, false);
            viewport.setScrollBarsShown(true, false);
            viewport.getVerticalScrollBar().setColour(juce::ScrollBar::thumbColourId, accentColor.withAlpha(0.3f));
            addAndMakeVisible(viewport);

            refreshList();
            
            LanguageManager::getInstance().addChangeListener(this);
            updateLanguage();
            updateTab(0);
        }

        ~ContentComponent() override
        {
            LanguageManager::getInstance().removeChangeListener(this);
        }

        void changeListenerCallback(juce::ChangeBroadcaster*) override
        {
            updateLanguage();
        }

        void updateLanguage()
        {
            btnControladores->setButtonText(TJS_L("TAB_CONTROLLERS"));
            btnConfig->setButtonText(TJS_L("TAB_SETTINGS"));
            btnIdiomas->setButtonText(TJS_L("TAB_LANGUAGES"));
            
            addBtn.setButtonText(TJS_L("BTN_ADD_CONTROLLER"));
            supportBtn->setButtonText(TJS_L("LINK_SUPPORT"));
            firmwareBtn->setButtonText(TJS_L("LINK_FIRMWARE"));
            
            searchBox.setTextToShowWhenEmpty(TJS_L("SEARCH_DEVICES"), juce::Colours::grey.withAlpha(0.4f));
            
            // Labels in header
            auto labels = tableHeader.getChildren();
            if (labels.size() >= 4) {
                static_cast<juce::Label*>(labels[0])->setText(TJS_L("COL_NAME"), juce::dontSendNotification);
                static_cast<juce::Label*>(labels[1])->setText(TJS_L("COL_MODEL"), juce::dontSendNotification);
                static_cast<juce::Label*>(labels[2])->setText(TJS_L("COL_VERSION"), juce::dontSendNotification);
                static_cast<juce::Label*>(labels[3])->setText(TJS_L("COL_ACTIONS"), juce::dontSendNotification);
                
                for (auto* l : labels)
                    if (auto* label = static_cast<juce::Label*>(l))
                        label->setMinimumHorizontalScale(0.7f); // Robustness
            }

            repaint();
        }

        void textEditorTextChanged(juce::TextEditor& ed) override
        {
            if (&ed == &searchBox) {
                if (currentTabId == 0) refreshList();
                else if (currentTabId == 2) refreshLanguageList();
            }
        }

        void paint(juce::Graphics& g) override
        {
            auto area = getLocalBounds();
            auto sidebarArea = area.removeFromLeft(sidebarWidth);

            // 1. Sidebar Background (Glass effect simulation)
            g.setColour(sidebarColor);
            g.fillRect(sidebarArea);
            
            g.setColour(juce::Colour(0xff2a2a2a));
            g.drawVerticalLine(sidebarArea.getRight() - 1, 0, (float)getHeight());

            // Logo Section
            auto logoArea = sidebarArea.removeFromTop(100).reduced(24, 0);
            g.setColour(juce::Colours::white);
            g.setFont(juce::Font("Inter", 20.0f, juce::Font::bold));
            auto logoLabelArea = logoArea.removeFromTop(40);
            g.drawText("Tridjs Live Suite", logoLabelArea, juce::Justification::bottomLeft);
            
            logoArea.removeFromTop(2); // 2px space
            g.setColour(juce::Colour(0xffa0a0a0));
            g.setFont(juce::Font("Inter", 12.0f, juce::Font::bold).withExtraKerningFactor(0.12f));
            g.drawText("GERENCIAMENTO", logoArea, juce::Justification::topLeft);

            // 2. Main Content Background
            g.setColour(mainBgColor);
            g.fillRect(area);

            // 3. Header Background (Sticky)
            auto headerArea = area.removeFromTop(headerHeight);
            g.setColour(mainBgColor.withAlpha(0.95f));
            g.fillRect(headerArea);
            g.setColour(juce::Colour(0xff2a2a2a));
            g.drawHorizontalLine(headerArea.getBottom() - 1, (float)headerArea.getX(), (float)headerArea.getRight());

            // Search Icon (Glass effect) - Only for controllers
            if (currentTabId == 0)
            {
                auto sArea = searchBox.getBounds().toFloat();
                g.setColour(juce::Colours::white.withAlpha(0.4f));
                juce::Path searchPath;
                float iconSize = 14.0f;
                float cx = sArea.getX() + 20, cy = sArea.getCentreY();
                searchPath.addEllipse(cx - (iconSize*0.4f), cy - (iconSize*0.4f), iconSize*0.8f, iconSize*0.8f);
                searchPath.startNewSubPath(cx + (iconSize*0.25f), cy + (iconSize*0.25f));
                searchPath.lineTo(cx + (iconSize*0.6f), cy + (iconSize*0.6f));
                g.strokePath(searchPath, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }

            // Avatar
            auto avatarArea = juce::Rectangle<float>(headerArea.getRight() - 50, (headerHeight - 32) / 2.0f, 32, 32);
            g.setColour(juce::Colour(0xff2a2a2a));
            g.fillEllipse(avatarArea);
            g.setColour(juce::Colour(0xff444444));
            g.drawEllipse(avatarArea, 1.0f);

            // 4. Content Area Titles
            auto contentArea = area.reduced(40, 0);
            contentArea.removeFromTop(40); // Top margin
            
            g.setColour(accentColor);
            g.setFont(juce::Font("Inter", 11.0f, juce::Font::bold).withExtraKerningFactor(0.08f));
            g.drawText(TJS_L("HW_MANAGER_SECTION"), contentArea.removeFromTop(20), juce::Justification::centredLeft);

            juce::String titleStr = TJS_L("TAB_CONTROLLERS");
            if (currentTabId == 1) titleStr = TJS_L("TAB_SETTINGS");
            else if (currentTabId == 2) titleStr = TJS_L("TAB_LANGUAGES");

            g.setColour(juce::Colours::white);
            g.setFont(juce::Font("Inter", 30.0f, juce::Font::bold));
            g.drawText(titleStr, contentArea.removeFromTop(45), juce::Justification::centredLeft);
            
            if (currentTabId != 0) {
                g.setColour(juce::Colour(0xffa0a0a0));
                g.setFont(juce::Font("Inter", 14.0f, juce::Font::plain));
                g.drawText(TJS_L("DEV_SECTION_MSG"), contentArea.removeFromTop(30), juce::Justification::centredLeft);
            }
        }

        void resized() override
        {
            auto area = getLocalBounds();
            auto sidebarArea = area.removeFromLeft(sidebarWidth);
            
            // Sidebar Layout
            sidebarArea.removeFromTop(100); // Space for logo
            btnControladores->setBounds(sidebarArea.removeFromTop(80));
            btnConfig->setBounds(sidebarArea.removeFromTop(80));
            btnIdiomas->setBounds(sidebarArea.removeFromTop(80));

            auto sidebarBottom = sidebarArea.removeFromBottom(160);
            addBtn.setBounds(sidebarBottom.removeFromTop(60).reduced(24, 10));
            sidebarBottom.removeFromTop(10); // border
            supportBtn->setBounds(sidebarBottom.removeFromTop(35).reduced(24, 0));
            firmwareBtn->setBounds(sidebarBottom.removeFromTop(35).reduced(24, 0));

            // Header Layout
            auto headerArea = area.removeFromTop(headerHeight);
            searchBox.setBounds(headerArea.getX() + 40, (headerHeight - 40) / 2, 450, 40);

            // Content Layout
            area.removeFromTop(140); // Titles space
            auto tableArea = area.reduced(40, 0);
            
            tableHeader.setBounds(tableArea.removeFromTop(50));
            auto labels = tableHeader.getChildren();
            int colWidth = tableArea.getWidth() / 4;
            for (int i = 0; i < labels.size(); ++i)
                labels[i]->setBounds(i * colWidth + 24, 0, colWidth - 24, 50);

            viewport.setBounds(tableArea.withTrimmedBottom(40));
            layoutContent();
        }

    private:
        const int sidebarWidth = 260;
        const int headerHeight = 64;
        const juce::Colour accentColor { 0xff00dbe9 };
        const juce::Colour sidebarColor { 0xff0a0a0a };
        const juce::Colour mainBgColor { 0xff121212 };

        struct NavButton : public juce::TextButton
        {
            NavButton(const juce::String& name, const juce::String& iconName, juce::Colour accent) 
                : juce::TextButton(name), icon(iconName), accentColor(accent) {}

            void paintButton(juce::Graphics& g, bool isMouseOver, bool isButtonDown) override
            {
                auto area = getLocalBounds();
                bool active = getToggleState();

                if (active) {
                    g.setColour(accentColor.withAlpha(0.1f));
                    g.fillRect(area);
                    g.setColour(accentColor);
                    g.fillRect(0, 0, 4, getHeight());
                } else if (isMouseOver) {
                    g.setColour(juce::Colours::white.withAlpha(0.05f));
                    g.fillRect(area);
                }

                auto contentArea = area.reduced(24, 0);
                g.setColour(active ? accentColor : (isMouseOver ? juce::Colours::white : juce::Colour(0xffa0a0a0)));
                
                // Unified icon alignment: Main buttons use larger area, footer links smaller
                bool isFooter = (getHeight() < 50);
                int iconSpace = isFooter ? 40 : 80;
                int iconSize = isFooter ? 24 : 48;
                
                auto iconArea = contentArea.removeFromLeft(iconSpace).withSizeKeepingCentre(iconSize, iconSize);
                drawIcon(g, icon, iconArea);
                
                g.setFont(juce::Font("Inter", isFooter ? 14.0f : 18.0f, juce::Font::plain));
                g.drawText(getButtonText(), contentArea, juce::Justification::centredLeft);
            }

            void drawIcon(juce::Graphics& g, const juce::String& type, juce::Rectangle<int> area)
            {
                auto fArea = area.toFloat().reduced(6);
                g.setColour(getToggleState() ? accentColor : (isMouseOver() ? juce::Colours::white : juce::Colour(0xffa0a0a0)));
                
                if (type == "settings_input_component") {
                    juce::Path p;
                    p.addRoundedRectangle(fArea.reduced(2), 2.0f);
                    p.addRectangle(fArea.getCentreX() - 1, fArea.getY() + 4, 2, 2);
                    p.addRectangle(fArea.getCentreX() - 1, fArea.getCentreY() - 1, 2, 2);
                    p.addRectangle(fArea.getCentreX() - 1, fArea.getBottom() - 6, 2, 2);
                    g.strokePath(p, juce::PathStrokeType(1.5f));
                } else if (type == "settings") {
                    g.drawEllipse(fArea.reduced(3), 2.0f);
                    for (int i = 0; i < 8; ++i) {
                        juce::Path p; p.addRoundedRectangle(-1.5f, -fArea.getWidth() * 0.45f, 3.0f, 4.0f, 1.0f);
                        p.applyTransform(juce::AffineTransform::rotation(i * juce::MathConstants<float>::pi / 4.0f).translated(fArea.getCentreX(), fArea.getCentreY()));
                        g.fillPath(p);
                    }
                } else if (type == "language") {
                    g.drawEllipse(fArea.reduced(1), 1.5f);
                    g.drawEllipse(fArea.getCentreX() - 3, fArea.getY() + 1, 6, fArea.getHeight() - 2, 1.0f);
                    g.drawLine(fArea.getX(), fArea.getCentreY(), fArea.getRight(), fArea.getCentreY(), 1.0f);
                } else if (type == "help") {
                    g.setFont(juce::Font("Inter", 16.0f, juce::Font::bold)); g.drawText("?", fArea, juce::Justification::centred);
                } else if (type == "update") {
                    g.drawEllipse(fArea.reduced(2), 1.5f);
                    juce::Path p; p.addTriangle(fArea.getCentreX() - 2, fArea.getCentreY() - 4, fArea.getCentreX() - 2, fArea.getCentreY() + 4, fArea.getCentreX() + 4, fArea.getCentreY());
                    g.fillPath(p);
                }
            }
            juce::String icon;
            juce::Colour accentColor;
        };

        struct ActionButton : public juce::TextButton
        {
            ActionButton(const juce::String& text, juce::Colour accent) : juce::TextButton(text), accentColor(accent) {}
            void paintButton(juce::Graphics& g, bool isMouseOver, bool isButtonDown) override {
                auto area = getLocalBounds().toFloat();
                g.setColour(isButtonDown ? accentColor.darker(0.2f) : (isMouseOver ? accentColor.brighter(0.1f) : accentColor));
                g.fillRoundedRectangle(area, 4.0f);
                g.setColour(juce::Colours::black);
                g.setFont(juce::Font("Inter", 11.0f, juce::Font::bold));
                g.drawText(getButtonText().toUpperCase(), area, juce::Justification::centred);
            }
            juce::Colour accentColor;
        };

        void setupNavButton(std::unique_ptr<NavButton>& btn, const juce::String& text, const juce::String& icon, int id)
        {
            btn = std::make_unique<NavButton>(text, icon, accentColor);
            btn->setRadioGroupId(100);
            btn->setClickingTogglesState(true);
            btn->onClick = [this, id] { updateTab(id); };
            addAndMakeVisible(btn.get());
        }

        void setupFooterLink(std::unique_ptr<NavButton>& btn, const juce::String& text, const juce::String& icon)
        {
            btn = std::make_unique<NavButton>(text, icon, accentColor);
            btn->setAlpha(0.6f);
            addAndMakeVisible(btn.get());
        }

        void updateTab(int id)
        {
            currentTabId = id;
            btnControladores->setToggleState(id == 0, juce::dontSendNotification);
            btnConfig->setToggleState(id == 1, juce::dontSendNotification);
            btnIdiomas->setToggleState(id == 2, juce::dontSendNotification);

            // Hide "Add" button when in Language tab
            addBtn.setVisible(id != 2);
            searchBox.setVisible(id == 0 || id == 2); 

            // Table content visibility
            tableHeader.setVisible(id == 0);
            viewport.setVisible(id == 0 || id == 2);

            if (id == 0) refreshList();
            else if (id == 2) refreshLanguageList();
            
            repaint();
        }
        void layoutContent()
        {
            auto contentWidth = viewport.getMaximumVisibleWidth();
            int itemHeight = 60;
            
            contentContainer.removeAllChildren();

            if (currentTabId == 0)
            {
                contentContainer.setBounds(0, 0, contentWidth, filteredFiles.size() * itemHeight);
                for (int i = 0; i < filteredFiles.size(); ++i)
                {
                    auto const& file = filteredFiles[i];
                    auto* row = new HardwareRowComponent(file, accentColor, [this, file] { onActivateCallback(file); });
                    row->setBounds(0, i * itemHeight, contentWidth, itemHeight);
                    contentContainer.addAndMakeVisible(row);
                }
            }
            else if (currentTabId == 2)
            {
                contentContainer.setBounds(0, 0, contentWidth, langFiles.size() * itemHeight);
                for (int i = 0; i < langFiles.size(); ++i)
                {
                    auto const& file = langFiles[i];
                    auto* row = new LanguageRowComponent(file, accentColor, [this, file] { 
                        LanguageManager::getInstance().loadLanguage(file);
                        refreshLanguageList(); 
                    });
                    row->setBounds(0, i * itemHeight, contentWidth, itemHeight);
                    contentContainer.addAndMakeVisible(row);
                }
            }
        }

        struct LanguageRowComponent : public juce::Component
        {
            LanguageRowComponent(const juce::File& f, juce::Colour accent, std::function<void()> onAct)
                : file(f), accentColor(accent)
            {
                if (auto xml = juce::XmlDocument::parse(file))
                    displayName = xml->getStringAttribute("name", file.getFileNameWithoutExtension());
                
                if (displayName.isEmpty()) displayName = file.getFileNameWithoutExtension();
                
                auto currentActiveFile = LanguageManager::getInstance().getCurrentFile();
                bool isActive = (currentActiveFile == file);
                
                activateBtn.onClick = onAct;
                activateBtn.setButtonText(TJS_L("BTN_ACTIVATE"));
                activateBtn.setEnabled(!isActive);
                activateBtn.setAlpha(isActive ? 0.3f : 1.0f);
                addAndMakeVisible(activateBtn);
            }

            void paint(juce::Graphics& g) override {
                auto area = getLocalBounds().reduced(8, 4);
                auto currentActiveFile = LanguageManager::getInstance().getCurrentFile();
                bool isActive = (currentActiveFile == file);
                
                // Rounded background for the row
                g.setColour(isActive ? accentColor.withAlpha(0.12f) : (isMouseOverRow ? juce::Colours::white.withAlpha(0.05f) : juce::Colour(0xff1a1a1a)));
                g.fillRoundedRectangle(area.toFloat(), 6.0f);

                if (isActive) {
                    g.setColour(accentColor);
                    g.drawRoundedRectangle(area.toFloat(), 6.0f, 1.5f);
                }

                auto textLines = area.reduced(20, 0);
                
                g.setColour(isActive ? accentColor : juce::Colours::white);
                g.setFont(juce::Font("Inter", 18.0f, juce::Font::bold));
                g.drawText(displayName, textLines.withTrimmedRight(200), juce::Justification::centredLeft);
                
                if (isActive) {
                    g.setColour(accentColor.withAlpha(0.8f));
                    g.setFont(juce::Font("Inter", 13.0f, juce::Font::italic));
                    juce::String activeStr = TJS_L("ACTIVE_STATUS");
                    if (activeStr == "ACTIVE_STATUS") activeStr = "Ativo";
                    
                    // Position status more toward the "middle" after the name
                    int nameWidth = juce::Font("Inter", 18.0f, juce::Font::bold).getStringWidth(displayName);
                    g.drawText("- " + activeStr, textLines.withTrimmedLeft(nameWidth + 20), juce::Justification::centredLeft);
                }
            }

            void resized() override {
                activateBtn.setBounds(getWidth() - 150, (getHeight() - 36) / 2, 120, 36);
            }

            void mouseEnter(const juce::MouseEvent&) override { isMouseOverRow = true; repaint(); }
            void mouseExit(const juce::MouseEvent&) override { isMouseOverRow = false; repaint(); }

            juce::File file;
            juce::Colour accentColor;
            ActionButton activateBtn { "", juce::Colour (0xff00dbe9) };
            juce::String displayName;
            bool isMouseOverRow = false;
        };

        struct HardwareRowComponent : public juce::Component
        {
            HardwareRowComponent(const juce::File& f, juce::Colour accent, std::function<void()> onAct)
                : file(f), accentColor(accent)
            {
                if (auto xml = juce::XmlDocument::parse(file)) {
                    version = xml->getStringAttribute("mixxxVersion", "1.0");
                    if (auto info = xml->getChildByName("info"))
                        displayName = info->getChildElementAllSubText("name", file.getFileNameWithoutExtension());
                }
                if (displayName.isEmpty()) displayName = file.getFileNameWithoutExtension();

                activateBtn.onClick = onAct;
                activateBtn.setButtonText(TJS_L("BTN_ACTIVATE"));
                addAndMakeVisible(activateBtn);
            }

            void paint(juce::Graphics& g) override {
                auto area = getLocalBounds().reduced(8, 4);
                
                // Premium rounded row background
                g.setColour(isMouseOverRow ? juce::Colours::white.withAlpha(0.05f) : juce::Colour(0xff1a1a1a));
                g.fillRoundedRectangle(area.toFloat(), 6.0f);

                auto colWidth = (area.getWidth() - 120) / 3;
                auto drawArea = area.reduced(20, 0);
                auto textCol = juce::Colours::white;
                
                // Nome
                auto nameArea = drawArea.removeFromLeft(colWidth);
                g.setColour(accentColor);
                g.fillEllipse((float)nameArea.getX(), (float)nameArea.getCentreY() - 4.0f, 8.0f, 8.0f);
                g.setColour(textCol);
                g.setFont(juce::Font("Inter", 16.0f, juce::Font::bold));
                g.drawText(displayName, nameArea.withTrimmedLeft(20), juce::Justification::centredLeft);

                // Modelo - Clean up .midi and Pioneer-
                auto modelArea = drawArea.removeFromLeft(colWidth);
                g.setColour(juce::Colour(0xffa0a0a0));
                g.setFont(juce::Font("JetBrains Mono", 12.0f, juce::Font::plain));
                juce::String modelName = file.getFileNameWithoutExtension().replace(".midi", "").replace("Pioneer-", "");
                g.drawText(modelName, modelArea, juce::Justification::centredLeft);

                // Versão
                auto versionArea = drawArea.removeFromLeft(colWidth);
                g.setColour(accentColor.withAlpha(0.8f));
                g.setFont(juce::Font("JetBrains Mono", 12.0f, juce::Font::plain));
                g.drawText("v" + version, versionArea, juce::Justification::centredLeft);
            }

            void resized() override {
                activateBtn.setBounds(getWidth() - 120, (getHeight() - 30) / 2, 100, 30);
            }

            void mouseEnter(const juce::MouseEvent&) override { isMouseOverRow = true; repaint(); }
            void mouseExit(const juce::MouseEvent&) override { isMouseOverRow = false; repaint(); }

            juce::File file;
            juce::Colour accentColor;
            ActionButton activateBtn { "", juce::Colour (0xff00dbe9) };
            juce::String displayName, version;
            bool isMouseOverRow = false;
        };

        void refreshLanguageList()
        {
            langFiles.clear();
            
            auto lFolder = LanguageManager::getLangDirectory();
            if (lFolder.exists()) {
                auto allLangs = lFolder.findChildFiles(juce::File::findFiles, false, "*.xml");
                juce::String filter = searchBox.getText().trim();
                juce::String currentLang = LanguageManager::getInstance().getCurrentLanguageName();
                
                struct LangInfo {
                    juce::File file;
                    juce::String name;
                };

                juce::Array<LangInfo> infoList;
                for (auto const& f : allLangs) {
                    juce::String name;
                    if (auto xml = juce::XmlDocument::parse(f))
                        name = xml->getStringAttribute("name");
                    
                    if (name.isEmpty()) name = f.getFileNameWithoutExtension();

                    if (filter.isEmpty() || name.containsIgnoreCase(filter)) {
                        // Avoid duplicate names in the list
                        bool nameExists = false;
                        for (auto& info : infoList) {
                            if (info.name == name) {
                                // If the new file is the one currently active, swap it in
                                if (f == LanguageManager::getInstance().getCurrentFile())
                                    info.file = f;
                                nameExists = true; 
                                break; 
                            }
                        }
                        if (!nameExists)
                            infoList.add({f, name});
                    }
                }

                // Sort: Active language first, then alphabetically
                struct LangInfoComparator {
                    juce::String current;
                    int compareElements(const LangInfo& a, const LangInfo& b) const {
                        if (a.name == b.name) return 0;
                        if (a.name == current) return -1;
                        if (b.name == current) return 1;
                        return a.name.compareIgnoreCase(b.name);
                    }
                };

                LangInfoComparator comp;
                comp.current = currentLang;
                infoList.sort(comp);

                for (auto const& info : infoList)
                    langFiles.add(info.file);
            }
            
            layoutContent();
        }

        void refreshList()
        {
            mappingFiles.clear();
            mappingFiles.addArray(targetFolder.findChildFiles(juce::File::findFiles, true, "*.midi.xml"));
            
            auto internalAlt = juce::File("C:\\TridjsMIDI\\Source\\Controllers\\Mappings");
            if (internalAlt.exists())
                mappingFiles.addArray(internalAlt.findChildFiles(juce::File::findFiles, true, "*.midi.xml"));

            // Remove duplicates
            for (int i = mappingFiles.size(); --i >= 0;)
                for (int j = 0; j < i; ++j)
                    if (mappingFiles[i].getFullPathName() == mappingFiles[j].getFullPathName()) { mappingFiles.remove(i); break; }

            filteredFiles.clear();
            juce::String filter = searchBox.getText().trim();
            for (auto const& f : mappingFiles)
                if (filter.isEmpty() || f.getFileName().containsIgnoreCase(filter))
                    filteredFiles.add(f);
            
            layoutContent();
        }

        void importMapping()
        {
            fileChooser = std::make_unique<juce::FileChooser>(TJS_L("FILE_CHOOSER_TITLE"), juce::File(), "*.midi.xml");
            fileChooser->launchAsync(juce::FileBrowserComponent::openMode, [this](const juce::FileChooser& fc) {
                auto file = fc.getResult();
                if (file.existsAsFile()) {
                    file.copyFileTo(targetFolder.getChildFile(file.getFileName()));
                    refreshList();
                }
            });
        }

        juce::File targetFolder;
        juce::Array<juce::File> mappingFiles, filteredFiles, langFiles;
        std::function<void(const juce::File&)> onActivateCallback;
        
        juce::Viewport viewport;
        juce::Component contentContainer;
        juce::TextEditor searchBox;
        juce::Component tableHeader;
        
        std::unique_ptr<NavButton> btnControladores, btnConfig, btnIdiomas, supportBtn, firmwareBtn;
        ActionButton addBtn { "Add New Controller", juce::Colour (0xff00dbe9) };

        int currentTabId = 0;
        std::unique_ptr<juce::FileChooser> fileChooser;
    };


    ControllerManagerWindow(const juce::File& mappingsDir, 
                             std::function<void(const juce::File&)> onActivate)
        : DocumentWindow("Manager",
                         juce::Colours::black,
                         DocumentWindow::allButtons)
    {
        setUsingNativeTitleBar(true);
        setResizable(true, true);
        setContentOwned(new ContentComponent(mappingsDir, onActivate), true);
        
        // Premium default size (Increased width by 20% to ~1100)
        centreWithSize(1100, 700);
    }

    void closeButtonPressed() override { setVisible(false); }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ControllerManagerWindow)
};
