#pragma once

#include <JuceHeader.h>
#include "ControllerProfile.h"

class BaseController
{
public:
    virtual ~BaseController() = default;

    virtual void loadProfile(const ControllerProfile& profile) = 0;
    virtual void start() = 0;
    virtual void stop() = 0;

    // Output methods for LED feedback
    virtual void handleOutputEvent(const ControllerOutputEvent& event) = 0;

protected:
    ControllerProfile currentProfile;
};
