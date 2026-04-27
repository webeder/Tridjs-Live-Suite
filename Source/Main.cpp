#include <JuceHeader.h>
#include "MainComponent.h"

class TridjsLiveSuiteApplication  : public juce::JUCEApplication
{
public:
    TridjsLiveSuiteApplication() {}

    const juce::String getApplicationName() override       { return ProjectInfo::projectName; }
    const juce::String getApplicationVersion() override    { return ProjectInfo::versionString; }
    bool moreThanOneInstanceAllowed() override             { return false; }

    void initialise (const juce::String& commandLine) override
    {
        mainWindow.reset (new MainWindow (getApplicationName()));
    }

    void shutdown() override
    {
        mainWindow = nullptr;
    }

    void systemRequestedQuit() override
    {
        quit();
    }

    class MainWindow    : public juce::DocumentWindow
    {
    public:
        MainWindow (juce::String name)
            : DocumentWindow (name,
                              juce::Colour(0xff1e1e1e), // Dark mode base theme colour
                              DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar (true);
            setContentOwned (new MainComponent(), true);

           #if JUCE_IOS || JUCE_ANDROID
            setFullScreen (true);
           #else
            setResizable (true, true);
            centreWithSize (getWidth(), getHeight());
           #endif

            setVisible (true);
        }

        void closeButtonPressed() override
        {
            if (auto* mc = dynamic_cast<MainComponent*>(getContentComponent()))
            {
                juce::AlertWindow::showOkCancelBox(juce::AlertWindow::QuestionIcon,
                    "Tridjs Live Suite",
                    "Deseja salvar o estado atual?",
                    "Sim", "Não", nullptr,
                    juce::ModalCallbackFunction::create([mc](int result){
                        if (result == 1) mc->saveAllSettings();
                        juce::JUCEApplication::getInstance()->systemRequestedQuit();
                    })
                );
            }
            else
            {
                juce::JUCEApplication::getInstance()->systemRequestedQuit();
            }
        }

    private:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainWindow)
    };

private:
    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION (TridjsLiveSuiteApplication)
