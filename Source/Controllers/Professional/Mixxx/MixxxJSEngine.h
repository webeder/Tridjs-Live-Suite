#pragma once

#include <JuceHeader.h>
#include "duktape.h"
#include "../../Core/ControllerRouter.h"
#include "../../Core/ControllerEvent.h"

/**
 * MixxxJSEngine
 * Bridges Mixxx-style JavaScript controller scripts to our C++ backend using Duktape.
 */
class MixxxJSEngine
{
public:
    // Per-deck scratch state (true = scratching, false = nudge/bend)
    std::atomic<bool> scratchingDecks[4] = { {false}, {false}, {false}, {false} };

    MixxxJSEngine(ControllerRouter& routerRef) : router(routerRef)
    {
        ctx = duk_create_heap_default();
        if (ctx) {
            // Store 'this' pointer in global stash for static C callbacks
            duk_push_global_stash(ctx);
            duk_push_pointer(ctx, this);
            duk_put_prop_string(ctx, -2, "engine_ptr");
            duk_pop(ctx);

            setupNativeFunctions();
        }
    }

    ~MixxxJSEngine()
    {
        if (ctx)
            duk_destroy_heap(ctx);
    }

    /** Executes a raw JS script string. */
    bool executeScript(const juce::String& jsCode)
    {
        const juce::ScopedLock lock (ctxLock);
        if (!ctx) return false;
        if (duk_peval_string(ctx, jsCode.toRawUTF8()) != 0) {
            juce::Logger::writeToLog("JS Error: " + juce::String(duk_safe_to_string(ctx, -1)));
            duk_pop(ctx);
            return false;
        }
        duk_pop(ctx);
        return true;
    }

    /** Calls a specific JS function (callback) by name. Supports dot notation (e.g. "MyObj.myFunc") */
    void callFunction(const juce::String& funcName, float value, const juce::String& group, const juce::String& key)
    {
        const juce::ScopedLock lock (ctxLock);
        if (!ctx) return;

        juce::StringArray parts;
        parts.addTokens(funcName, ".", "");

        if (parts.size() == 0) return;

        duk_push_global_object(ctx);

        for (int i = 0; i < parts.size(); ++i)
        {
            duk_get_prop_string(ctx, -1, parts[i].toRawUTF8());
            if (duk_is_undefined(ctx, -1))
            {
                duk_pop_2(ctx);
                return;
            }
            if (i < parts.size() - 1)
                duk_remove(ctx, -2);
        }

        if (duk_is_function(ctx, -1))
        {
            duk_push_number(ctx, value);
            duk_push_string(ctx, group.toRawUTF8());
            duk_push_string(ctx, key.toRawUTF8());
            if (duk_pcall(ctx, 3) != 0) {
                juce::Logger::writeToLog("JS Call Error (" + funcName + "): " + juce::String(duk_safe_to_string(ctx, -1)));
            }
        }
        duk_pop_2(ctx);
    }

    /** Calls a Mixxx-style script binding with full MIDI parameters:
        function(channel, control, value, status, group) */
    void callMixxxFunction(const juce::String& funcName, int channel, int control, int value, int status, const juce::String& group)
    {
        const juce::ScopedLock lock (ctxLock);
        if (!ctx) return;

        juce::StringArray parts;
        parts.addTokens(funcName, ".", "");
        if (parts.size() == 0) return;

        duk_push_global_object(ctx);
        for (int i = 0; i < parts.size(); ++i)
        {
            duk_get_prop_string(ctx, -1, parts[i].toRawUTF8());
            if (duk_is_undefined(ctx, -1)) { duk_pop_2(ctx); return; }
            if (i < parts.size() - 1) duk_remove(ctx, -2);
        }

        if (duk_is_function(ctx, -1))
        {
            duk_push_number(ctx, (double)channel);
            duk_push_number(ctx, (double)control);
            duk_push_number(ctx, (double)value);
            duk_push_number(ctx, (double)status);
            duk_push_string(ctx, group.toRawUTF8());
            if (duk_pcall(ctx, 5) != 0) {
                juce::Logger::writeToLog("JS Call Error (" + funcName + "): " + juce::String(duk_safe_to_string(ctx, -1)));
            }
        }
        duk_pop_2(ctx);
    }

private:
    static MixxxJSEngine* getSelf(duk_context* c) {
        duk_push_global_stash(c);
        duk_get_prop_string(c, -1, "engine_ptr");
        auto* self = (MixxxJSEngine*)duk_get_pointer(c, -1);
        duk_pop_2(c);
        return self;
    }

    void setupNativeFunctions()
    {
        // 1. print(msg)
        duk_push_c_function(ctx, [](duk_context* c) -> duk_ret_t {
            juce::Logger::writeToLog("JS: " + juce::String(duk_safe_to_string(c, 0)));
            return 0;
        }, 1);
        duk_put_global_string(ctx, "print");

        // 2. 'engine' object
        duk_push_object(ctx);

        // engine.setValue(group, key, value)
        duk_push_c_function(ctx, [](duk_context* c) -> duk_ret_t {
            auto* self = getSelf(c);
            juce::String group = duk_get_string(c, 0);
            juce::String key = duk_get_string(c, 1);
            float val = (float)duk_get_number(c, 2);

            ControllerInputEvent ev;
            ev.deckIndex = self->parseDeckFromGroup(group);
            ev.value = val;
            ev.type = self->mapMixxxKeyToEventType(key);

            if (ev.type != ControllerEventType::Unknown)
                self->router.pushEvent(ev);
            return 0;
        }, 3);
        duk_put_prop_string(ctx, -2, "setValue");

        // engine.getValue(group, key)
        duk_push_c_function(ctx, [](duk_context* c) -> duk_ret_t {
            duk_push_number(c, 0.0);
            return 1;
        }, 2);
        duk_put_prop_string(ctx, -2, "getValue");

        // engine.scratchEnable(deck, intervals, rpm, alpha, beta)
        duk_push_c_function(ctx, [](duk_context* c) -> duk_ret_t {
            auto* self = getSelf(c);
            int deck = duk_get_int(c, 0); // 1-based
            ControllerInputEvent ev;
            ev.type = ControllerEventType::ScratchMode;
            ev.deckIndex = deck - 1;
            ev.value = 1.0f; // Enable
            // Track scratch state
            if (deck >= 1 && deck <= 4)
                self->scratchingDecks[deck - 1].store(true);
            self->router.pushEvent(ev);
            return 0;
        }, 5);
        duk_put_prop_string(ctx, -2, "scratchEnable");

        // engine.scratchTick(deck, delta)
        duk_push_c_function(ctx, [](duk_context* c) -> duk_ret_t {
            auto* self = getSelf(c);
            int deck = duk_get_int(c, 0);
            float delta = (float)duk_get_number(c, 1);
            ControllerInputEvent ev;
            ev.type = ControllerEventType::JogScratch;
            ev.deckIndex = deck - 1;
            ev.value = delta;
            self->router.pushEvent(ev);
            return 0;
        }, 2);
        duk_put_prop_string(ctx, -2, "scratchTick");

        // engine.isScratching(deck)
        duk_push_c_function(ctx, [](duk_context* c) -> duk_ret_t {
            auto* self = getSelf(c);
            int deck = duk_get_int(c, 0); // 1-based
            bool scratching = false;
            if (deck >= 1 && deck <= 4)
                scratching = self->scratchingDecks[deck - 1].load();
            duk_push_boolean(c, scratching);
            return 1;
        }, 1);
        duk_put_prop_string(ctx, -2, "isScratching");

        // engine.scratchDisable(deck)
        duk_push_c_function(ctx, [](duk_context* c) -> duk_ret_t {
            auto* self = getSelf(c);
            int deck = duk_get_int(c, 0); // 1-based
            ControllerInputEvent ev;
            ev.type = ControllerEventType::ScratchMode;
            ev.deckIndex = deck - 1;
            ev.value = 0.0f; // Disable
            // Track scratch state
            if (deck >= 1 && deck <= 4)
                self->scratchingDecks[deck - 1].store(false);
            self->router.pushEvent(ev);
            return 0;
        }, 1);
        duk_put_prop_string(ctx, -2, "scratchDisable");

        duk_put_global_string(ctx, "engine");

        // 3. 'script' object with deckFromGroup
        duk_push_object(ctx);
        duk_push_c_function(ctx, [](duk_context* c) -> duk_ret_t {
            juce::String group = duk_get_string(c, 0);
            int match = group.indexOf("[Channel") + 8;
            int end = group.indexOf(match, "]");
            if (end > match) {
                duk_push_number(c, group.substring(match, end).getIntValue());
                return 1;
            }
            duk_push_null(c);
            return 1;
        }, 1);
        duk_put_prop_string(ctx, -2, "deckFromGroup");
        duk_put_global_string(ctx, "script");

        // 4. 'midi' object with sendShortMsg (output only - logs for now)
        duk_push_object(ctx);
        duk_push_c_function(ctx, [](duk_context* c) -> duk_ret_t {
            int status = duk_get_int(c, 0);
            int data1 = duk_get_int(c, 1);
            int data2 = duk_get_int(c, 2);
            juce::Logger::writeToLog("MIDI OUT: " + juce::String::toHexString(status) + " " +
                                     juce::String::toHexString(data1) + " " +
                                     juce::String::toHexString(data2));
            return 0;
        }, 3);
        duk_put_prop_string(ctx, -2, "sendShortMsg");
        duk_put_global_string(ctx, "midi");

        // 5. 'midi' stub methods (mixxx standard)
        duk_eval_string(ctx, "midi.makeInputHandler = function(){};");

        // 6. 'engine' stub methods (mixxx standard)
        duk_eval_string(ctx, "engine.makeConnection = function(){}; engine.beginTimer = function(){ return 0; }; engine.stopTimer = function(){}; engine.softTakeover = function(){};");
        duk_pop(ctx);

        // 7. 'components' stub (prevents init errors)
        duk_eval_string(ctx, "var components = { Component: function(){}, ComponentContainer: function(){} };");
        duk_pop(ctx);
    }

    int parseDeckFromGroup (const juce::String& group)
    {
        if (group.startsWithIgnoreCase ("[Channel"))
            return group.substring (8, group.length() - 1).getIntValue() - 1;
        return 0;
    }

    ControllerEventType mapMixxxKeyToEventType (const juce::String& key)
    {
        if (key == "play")            return ControllerEventType::Play;
        if (key == "cue_default")     return ControllerEventType::Cue;
        if (key == "volume")          return ControllerEventType::Volume;
        if (key == "pregain")         return ControllerEventType::Gain;
        if (key == "rate")            return ControllerEventType::Pitch;
        if (key.containsIgnoreCase ("filter") || key == "super1") return ControllerEventType::Filter;
        if (key.containsIgnoreCase ("parameter1")) return ControllerEventType::EQLow;
        if (key.containsIgnoreCase ("parameter2")) return ControllerEventType::EQMid;
        if (key == "parameter3") return ControllerEventType::EQHigh;
        if (key == "loop_in")         return ControllerEventType::LoopIn;
        if (key == "loop_out")        return ControllerEventType::LoopOut;
        if (key == "reloop_toggle")   return ControllerEventType::Reloop;
        if (key == "loop_halve")      return ControllerEventType::LoopHalve;
        if (key == "loop_double")     return ControllerEventType::LoopDouble;
        if (key == "jog" || key == "wheel") return ControllerEventType::JogBend;
        return ControllerEventType::Unknown;
    }

    duk_context* ctx = nullptr;
    ControllerRouter& router;
    juce::CriticalSection ctxLock;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MixxxJSEngine)
};
