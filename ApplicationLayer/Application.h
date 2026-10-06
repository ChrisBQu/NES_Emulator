#ifndef APPLICATION_H
#define APPLICATION_H

#include "../EmulationLayer/Bus.h"
#include "../EmulationLayer/Cartridge.h"
#include "Screen.h"
#include "Overlay.h"
#include "Rewind.h"
#include "Config.h"

struct ApplicationState {
    struct NES_Console *console;
    struct Screen *screen;
    struct Overlay *overlay;
    struct Rewind *rewind;
    struct Cartridge *cartridge;
    struct Controller* controller_p1;
    struct Controller* controller_p2;
    SDL_AudioDeviceID sdlAudioDevice;

    struct Config options;

    // The console runs a number of startup cycles before the game starts
    // startup_ready  is true once they are complete
    uint16_t startup_cycles;
    bool startup_ready;

    // Id of the "Fast forwarding..." message while it's shown, otherwise OVERLAY_INVALID_ID
    int turbo_message_id;

    // Frame pacing for when there is no audio device: the time the next frame is due, and 
    // the length of a frame, in performance counter ticks
    Uint64 perf_frequency;
    Uint64 frame_ticks;
    Uint64 next_frame_time;
};

struct ApplicationState* initApplicationState();
void tickMain(struct ApplicationState *appState);
void destroyApplicationState(struct ApplicationState *appState);

// Write the options, including the controllers' current bindings, to the config file
void saveApplicationOptions(struct ApplicationState *appState);

// Draw the frame and overlay, and show them. tickMain does this every frame, but it's paused while a dialog is open,
// so a dialog that changes how the screen looks can call this to show the change
void presentAll(struct ApplicationState *appState);

struct ApplicationState* getGlobalApplicationState();

#endif