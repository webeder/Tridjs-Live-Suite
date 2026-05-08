#pragma once

#include <JuceHeader.h>
#include "ControllerEvent.h"

// A Realtime-Safe Lock-Free Event Bus
class ControllerRouter
{
public:
    ControllerRouter() : fifo(1024) {}

    // Called by the Controller Pipeline (Mixxx or Custom) - Safe for MIDI/Serial threads
    void pushEvent(const ControllerInputEvent& event)
    {
        int start1, size1, start2, size2;
        fifo.prepareToWrite(1, start1, size1, start2, size2);
        
        if (size1 > 0)
        {
            events[start1] = event;
        }
        else if (size2 > 0)
        {
            events[start2] = event;
        }
        
        fifo.finishedWrite(size1 + size2);
    }

    // Called by the Audio Thread or a fast Timer to process events safely without blocking
    bool popEvent(ControllerInputEvent& event)
    {
        int start1, size1, start2, size2;
        fifo.prepareToRead(1, start1, size1, start2, size2);
        
        bool popped = false;
        if (size1 > 0)
        {
            event = events[start1];
            popped = true;
        }
        else if (size2 > 0)
        {
            event = events[start2];
            popped = true;
        }
        
        fifo.finishedRead(popped ? 1 : 0);
        return popped;
    }

private:
    juce::AbstractFifo fifo;
    ControllerInputEvent events[1024];
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ControllerRouter)
};
