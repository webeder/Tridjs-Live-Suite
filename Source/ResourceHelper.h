#pragma once
#include <JuceHeader.h>

inline juce::File findResourceFile(const juce::String& filename)
{
    auto exeDir = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory();

    // Next to executable (deployed)
    auto f1 = exeDir.getChildFile(filename);
    if (f1.existsAsFile()) return f1;

    // Relative to executable (development: build/Debug/ -> source root)
    auto f2 = exeDir.getParentDirectory().getParentDirectory()
                  .getChildFile("Tridjs-Live-Suite-main").getChildFile(filename);
    if (f2.existsAsFile()) return f2;

    return {};
}

inline juce::File findMappingsDir()
{
    auto exeDir = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory();

    // 1. Deployed: next to executable
    auto d1 = exeDir.getChildFile("Mappings");
    if (d1.exists()) return d1;

    // 2. Development: source tree
    auto d2 = exeDir.getParentDirectory().getParentDirectory()
                  .getChildFile("Tridjs-Live-Suite-main")
                  .getChildFile("Source").getChildFile("Controllers").getChildFile("Mappings");
    if (d2.exists()) return d2;

    return {};
}
