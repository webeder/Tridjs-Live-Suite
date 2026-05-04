#pragma once
#include <JuceHeader.h>

class VstScannerThread : public juce::ThreadWithProgressWindow
{
public:
    VstScannerThread(juce::KnownPluginList& list, juce::AudioPluginFormatManager& fm, std::function<void(bool)> onComplete)
        : ThreadWithProgressWindow("Rescan All Plugins...", true, true),
          knownPluginList(list), formatManager(fm), onCompleteCallback(std::move(onComplete))
    {
    }

    void run() override
    {
        juce::FileSearchPath path;
        path.add(juce::File("C:\\Program Files\\Common Files\\VST3"));
        path.add(juce::File("C:\\Program Files\\VSTPlugins"));
        path.add(juce::File("C:\\Program Files\\Common Files\\VST2"));
        path.add(juce::File("C:\\Program Files\\Common Files\\Steinberg\\VST2"));
        path.add(juce::File("C:\\Program Files\\Steinberg\\VSTPlugins"));
        path.add(juce::File(juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("VST3")));
        path.add(juce::File(juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("VSTPlugins")));

        for (int i = 0; i < formatManager.getNumFormats(); ++i)
        {
            auto* format = formatManager.getFormat(i);
            
            juce::PluginDirectoryScanner scanner(knownPluginList, *format, path, true, 
                juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("tridjs_vst_scanner.txt"));

            juce::String name;
            while (scanner.scanNextFile(true, name))
            {
                if (threadShouldExit()) return;
                setProgress(0.5); 
                setStatusMessage("Escaneando: " + name);
            }
        }
    }

    void threadComplete(bool userPressedCancel) override
    {
        if (onCompleteCallback)
            onCompleteCallback(userPressedCancel);
    }

private:
    juce::KnownPluginList& knownPluginList;
    juce::AudioPluginFormatManager& formatManager;
    std::function<void(bool)> onCompleteCallback;
};

class PluginScannerManager : public juce::ChangeBroadcaster
{
public:
    PluginScannerManager()
    {
        formatManager.addDefaultFormats();
        loadKnownPlugins();
        validatePlugins();
    }

    ~PluginScannerManager()
    {
        saveKnownPlugins();
    }

    void rescan()
    {
        if (scannerThread != nullptr && scannerThread->isThreadRunning())
            return; // Already scanning

        knownPluginList.clear();
        
        scannerThread = std::make_unique<VstScannerThread>(
            knownPluginList, 
            formatManager, 
            [this](bool cancelled) 
            {
                if (!cancelled)
                {
                    saveKnownPlugins();
                    sendChangeMessage();
                    juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon, 
                        "Rescan Concluido", "A biblioteca de plugins foi atualizada com sucesso!");
                }
                
                // Clean up the thread object safely on the message thread
                juce::MessageManager::callAsync([this]() {
                    scannerThread.reset();
                });
            }
        );
        
        scannerThread->launchThread();
    }

    void validatePlugins()
    {
        juce::StringArray missingPlugins;
        auto types = knownPluginList.getTypes();
        
        // We need to iterate backwards or collect and then remove to avoid index issues
        for (int i = types.size() - 1; i >= 0; --i)
        {
            auto desc = types[i];
            juce::File pluginFile(desc.fileOrIdentifier);
            
            // VST3 plugins can be directories (bundles), so we must use exists() instead of existsAsFile()
            if (!pluginFile.exists())
            {
                missingPlugins.add(desc.name + " (" + desc.fileOrIdentifier + ")");
                knownPluginList.removeType(desc);
            }
        }

        if (missingPlugins.size() > 0)
        {
            juce::String msg = "Os seguintes plugins nao foram encontrados e foram removidos da lista:\n\n";
            msg += missingPlugins.joinIntoString("\n");
            
            juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, 
                "Plugins Ausentes", msg);
            
            saveKnownPlugins();
            sendChangeMessage();
        }
    }

    void loadKnownPlugins()
    {
        auto file = getSettingsFile();
        if (file.existsAsFile())
        {
            if (auto xml = juce::parseXML(file))
                knownPluginList.recreateFromXml(*xml);
        }
    }

    void saveKnownPlugins()
    {
        if (auto xml = knownPluginList.createXml())
        {
            juce::File dir = getSettingsFile().getParentDirectory();
            if (!dir.exists()) dir.createDirectory();
            xml->writeTo(getSettingsFile());
        }
    }

    juce::KnownPluginList& getKnownPluginList() { return knownPluginList; }
    juce::AudioPluginFormatManager& getFormatManager() { return formatManager; }

    static juce::File getSettingsFile()
    {
        return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
            .getChildFile("Tridjs")
            .getChildFile("PluginsSettings.xml");
    }

private:
    juce::AudioPluginFormatManager formatManager;
    juce::KnownPluginList knownPluginList;
    std::unique_ptr<VstScannerThread> scannerThread;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginScannerManager)
};
