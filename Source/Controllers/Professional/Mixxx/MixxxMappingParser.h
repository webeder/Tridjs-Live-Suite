#pragma once

#include <JuceHeader.h>
#include <map>
#include "../../../Core/MixxxMappingParser.h"

// Professional Mixxx mapping parser.
// Delegates to the Core parser for XML loading and control lookup.
class ProfessionalMixxxMappingParser
{
public:
    ProfessionalMixxxMappingParser() = default;

    bool loadMapping(const juce::File& xmlFile, bool parseScripts = false)
    {
        if (!xmlFile.existsAsFile())
            return false;

        bool ok = coreParser.loadXml(xmlFile);

        if (ok && parseScripts)
            scriptFile = xmlFile.getParentDirectory().getChildFile(
                xmlFile.getFileNameWithoutExtension() + "-script.js");

        return ok;
    }

    MixxxControl getControl(int status, int midino) const
    {
        return coreParser.getControl(status, midino);
    }

    size_t getMappingCount() const
    {
        return coreParser.getMappingCount();
    }

    juce::File getScriptFile() const { return scriptFile; }

private:
    MixxxMappingParser coreParser;
    juce::File scriptFile;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ProfessionalMixxxMappingParser)
};
