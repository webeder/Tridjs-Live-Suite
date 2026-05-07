#include <JuceHeader.h>
#include "BinaryData.h"
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
        // Explicitly decode the PNG from memory
        juce::MemoryInputStream stream (BinaryData::splash2_png, BinaryData::splash2_pngSize, false);
        juce::PNGImageFormat pngFormat;
        juce::Image logo = pngFormat.decodeImage(stream);
        
        // If the image failed to decode, create a fallback image
        if (!logo.isValid()) {
             logo = juce::Image(juce::Image::RGB, 700, 327, true);
             juce::Graphics g(logo);
             g.fillAll(juce::Colours::black);
             g.setColour(juce::Colours::red);
             g.drawText("ERRO: splash2.png invalido no BinaryData", logo.getBounds(), juce::Justification::centred);
        }

        // Show splash screen
        splash.reset(new juce::SplashScreen(getApplicationName(), logo, true));
        
        mainWindow.reset (new MainWindow (getApplicationName()));
        
        if (splash)
            splash->deleteAfterDelay(juce::RelativeTime::seconds(3.0), false);
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
            setContentOwned (static_cast<juce::Component*> (new MainComponent()), true);

           #if JUCE_IOS || JUCE_ANDROID
            setFullScreen (true);
           #else
            setResizable (true, true);
            setResizeLimits (1024, 700, 32768, 32768);
            setFullScreen (true);
           #endif

            setVisible (true);
        }

        void closeButtonPressed() override
        {
            if (auto* mc = dynamic_cast<MainComponent*>(getContentComponent()))
            {
                juce::AlertWindow::showOkCancelBox(juce::AlertWindow::QuestionIcon,
                    "Tridjs Live Suite",
                    juce::String::fromUTF8("Deseja salvar o estado atual da sessão?"),
                    "Sim", juce::String::fromUTF8("Não"), nullptr,
                    juce::ModalCallbackFunction::create([mc](int result){
                        // result == 1 is OK (Sim), result == 0 is Cancel (Não)
                        mc->setSaveOnQuit(result == 1);
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
    std::unique_ptr<juce::SplashScreen> splash;
};

START_JUCE_APPLICATION (TridjsLiveSuiteApplication)
