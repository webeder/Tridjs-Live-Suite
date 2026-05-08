#pragma once

#include <JuceHeader.h>
#include "../../../Core/ControllerRouter.h"

// Wrapper around juce::JavascriptEngine to execute Mixxx scripts
class MixxxJSEngine
{
public:
    MixxxJSEngine(ControllerRouter& routerRef) : router(routerRef)
    {
        // Register native C++ callbacks for the JS environment
        setupNativeFunctions();
    }

    bool executeScript(const juce::String& jsCode)
    {
        auto result = jsEngine.execute(jsCode);
        return !result.isError();
    }

private:
    void setupNativeFunctions()
    {
        // TODO: Bind Mixxx's engine.setValue, engine.getValue, print, etc.
    }

    juce::JavascriptEngine jsEngine;
    ControllerRouter& router;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MixxxJSEngine)
};
