#pragma once

#include <JuceHeader.h>

/**
 * LanguageManager
 * Handles XML-based localization with ChangeBroadcaster for hot-reloading.
 * Safe for UI use, but NEVER call TJS_L() in the Audio Thread.
 */
class LanguageManager : public juce::ChangeBroadcaster
{
public:
    static LanguageManager& getInstance()
    {
        static LanguageManager instance;
        return instance;
    }

    /** Loads a language XML file and notifies all listeners. */
    void loadLanguage(const juce::File& xmlFile)
    {
        juce::ScopedLock lock (mapLock); 
        currentMap.clear();
        languageName = "Default";
        currentFile = xmlFile;

        if (!xmlFile.existsAsFile()) {
            sendChangeMessage();
            return;
        }

        // Force UTF-8 interpretation
        juce::XmlDocument doc (xmlFile);
        if (auto xml = doc.getDocumentElement())
        {
            if (xml->hasTagName("language"))
            {
                languageName = xml->getStringAttribute("name", xmlFile.getFileNameWithoutExtension());
                
                auto* entry = xml->getChildByName("entry");
                while (entry != nullptr)
                {
                    auto key = entry->getStringAttribute("key");
                    auto value = entry->getStringAttribute("value");
                    
                    if (key.isNotEmpty() && value.isNotEmpty())
                        currentMap.set(key, value);
                    
                    entry = entry->getNextElementWithTagName("entry");
                }
            }
        }
        
        // Notify all components that the language has changed (even if failed/cleared)
        sendChangeMessage();
    }

    /** 
     * Centralized lookup for the 'lang' directory across different environments.
     */
    static juce::File getLangDirectory()
    {
        auto exeDir = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory();
        
        // 1. Next to the running executable
        auto d1 = exeDir.getChildFile("lang");
        if (d1.exists()) return d1;

        // 2. Current working directory
        auto d2 = juce::File::getCurrentWorkingDirectory().getChildFile("lang");
        if (d2.exists()) return d2;

        // 3. Source-tree fallback (development)
        auto d3 = exeDir.getParentDirectory().getParentDirectory()
                       .getChildFile("Tridjs-Live-Suite-main").getChildFile("lang");
        if (d3.exists()) return d3;

        // 4. Source-tree fallback (legacy)
        auto d4 = exeDir.getParentDirectory().getParentDirectory()
                         .getParentDirectory().getParentDirectory()
                         .getChildFile("lang");
        if (d4.exists()) return d4;

        return {};
    }

    /** 
     * Translates a key. 
     * WARNING: This involves map lookup and string allocation.
     * Use only on UI thread. Cache results for performance.
     */
    juce::String translate(const juce::String& key, const juce::String& defaultValue = "")
    {
        juce::ScopedLock lock (mapLock);
        
        juce::String val = currentMap.getValue(key, "__NOT_FOUND__");
        
        if (val != "__NOT_FOUND__" && val.isNotEmpty())
            return val;
        
        return defaultValue.isNotEmpty() ? defaultValue : key;
    }

    juce::String getCurrentLanguageName() const { return languageName; }
    juce::File getCurrentFile() const { return currentFile; }
    
    void clearTranslations() { loadLanguage(juce::File()); }

private:
    LanguageManager() 
    {
        currentMap.set("APP_NAME", "Tridjs Live Suite");
    }
    
    juce::CriticalSection mapLock; // Protect the map during UI updates
    juce::StringPairArray currentMap;
    juce::String languageName = "Default";
    juce::File currentFile;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LanguageManager)
};

#define TJS_L(key) LanguageManager::getInstance().translate(key)
