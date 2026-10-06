#include "Application.h"
#include "Controller.h"
#include "Window.h"
#include "../EmulationLayer/APU.h"
#include "../EmulationLayer/PPU.h"
#include "../EmulationLayer/BatterySave.h"
#include "../EmulationLayer/Savestate.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct ApplicationState* globalAppState;

#define NES_FRAMES_PER_SECOND 60.0988
#define TURBO_SPEED 4

// Audio samples produced by the APU during the current frame; queued to SDL once the frame is done (~735 per frame at 44.1kHz)
#define AUDIO_SAMPLE_RATE 44100
#define AUDIO_BUFFER_SIZE 4096
float audio_buffer[AUDIO_BUFFER_SIZE];
int audio_buffer_count = 0;

// With audio, the emulator is paced by the sound card: it only runs another frame once the queued audio drops below
// this amount (~50ms). This keeps sound and emulation in sync without pops or growing delay
const Uint32 audio_target_bytes = (AUDIO_SAMPLE_RATE / 20) * sizeof(float);

// Create an audio function that will plug into the emulator
void receiveSample(float sample) {
    if (audio_buffer_count < AUDIO_BUFFER_SIZE) {
        audio_buffer[audio_buffer_count++] = sample;
    }
}

// Create a rendering function that will plug into the emulator
// Each NES pixel is one framebuffer pixel; the GPU scales the finished frame up to the window's size
void receivePixel(struct NES_Pixel pxl) {
    struct ApplicationState* appState = getGlobalApplicationState();
    uint32_t color = 0xFF000000u | ((uint32_t)pxl.r << 16) | ((uint32_t)pxl.g << 8) | pxl.b;
    appState->screen->framebuffer[pxl.y * appState->screen->width + pxl.x] = color;
}


struct ApplicationState* initApplicationState() {

    // There should only be one global application state
    if (globalAppState != NULL) { destroyApplicationState(globalAppState); }

    // Zeroed, so if a part fails to initialize, destroyApplicationState only frees the parts that were created
    struct ApplicationState *appState = calloc(1, sizeof(struct ApplicationState));
    if (appState == NULL) {
        return NULL;
    }

    // Load the config file. Options missing from it keep their defaults, and if there's no file, one is made with the defaults
    setConfigDefaults(&appState->options);
    if (loadConfigFile(&appState->options, "config.cfg") != 0) {
        if (saveConfigFile(&appState->options, "config.cfg") != 0) { printf("Warning: Could not create the config file.\n"); }
    }

    // Initialize the console
    appState->console = NES_initConsole();
    if (appState->console == NULL) {
        destroyApplicationState(appState);
        return NULL;
    }

    // Initialize the screen
    appState->screen = createScreen(DEFAULT_SCREEN_WIDTH, DEFAULT_SCREEN_HEIGHT);
    if (appState->screen == NULL) {
        destroyApplicationState(appState);
        return NULL;
    }
    setScreenShader(appState->screen, appState->options.crtFilterEnabled ? SCREEN_SHADER_CRT : SCREEN_SHADER_NORMAL);
    setCrtSettings(appState->screen, &appState->options.crt);

    // Initialize the overlay
    appState->overlay = createOverlay();
    if (appState->overlay == NULL) {
        destroyApplicationState(appState);
        return NULL;
    }

    // Initialize the rewind
    appState->rewind = createRewind(DEFAULT_SCREEN_WIDTH, DEFAULT_SCREEN_HEIGHT);
    if (appState->rewind == NULL) {
        destroyApplicationState(appState);
        return NULL;
    }

    // Initialize the SDL audio device
    appState->sdlAudioDevice = 0;
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) == 0) {
        SDL_AudioSpec want = { 0 };
        want.freq = AUDIO_SAMPLE_RATE;
        want.format = AUDIO_F32SYS;
        want.channels = 1;
        want.samples = 1024;
        want.callback = NULL;
        appState->sdlAudioDevice = SDL_OpenAudioDevice(NULL, 0, &want, NULL, 0);
    }
    if (appState->sdlAudioDevice != 0) {
        NES_APU_setSampleRate(appState->console->ConnectedAPU, AUDIO_SAMPLE_RATE);
        appState->console->audioOutFunc = receiveSample;
        SDL_PauseAudioDevice(appState->sdlAudioDevice, 0);  // Start playback
    }
    else {
        printf("Warning: Could not open an audio device, running without sound. SDL Error: %s\n", SDL_GetError());
    }

    // Bind the controllers to the application state, with default bindings
    appState->controller_p1 = create_controller();
    appState->controller_p2 = create_controller();
    controller_init_default_bindings(appState->controller_p1);

    // Replace the default bindings with any from the config file
    struct Controller* controllers[2] = { appState->controller_p1, appState->controller_p2 };
    for (int player = 0; player < 2; player++) {
        for (int b = 0; b < NUMBER_OF_BUTTONS; b++) {
            const char* name = appState->options.bindings[player][b];
            if (name == NULL) { continue; }
            if (!controller_bind_from_string(controllers[player], name, (CONTROLLER_BUTTON)b)) {
                printf("Warning: Unknown binding \"%s\" in the config file, keeping the default.\n", name);
            }
        }
    }

    // Without audio, frame pacing falls back to a timer
    appState->turbo_message_id = OVERLAY_INVALID_ID;
    appState->perf_frequency = SDL_GetPerformanceFrequency();
    appState->frame_ticks = (Uint64)(appState->perf_frequency / NES_FRAMES_PER_SECOND);
    appState->next_frame_time = 0;

    // Start gamepad support. The controllers open their pads when SDL reports them, including pads already plugged in
    if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) != 0) {
        printf("Warning: Could not start gamepad support, only the keyboard will work. SDL Error: %s\n", SDL_GetError());
    }

    // Clear once before the first frame
    clearScreen(appState->screen);    
    
    // Attach the rendering function
    appState->console->imageOutFunc = *receivePixel;

    globalAppState = appState;
    return appState;
}

void destroyApplicationState(struct ApplicationState *appState) {
    if (appState == NULL) { return; }
    NES_writeBatterySave(appState->console->ConnectedCartridge);
    destroyRewind(appState->rewind);
    destroyScreen(appState->screen);
    destroyOverlay(appState->overlay);
    if (appState->sdlAudioDevice != 0) { SDL_CloseAudioDevice(appState->sdlAudioDevice); }
    destroy_controller(appState->controller_p1);
    destroy_controller(appState->controller_p2);
    NES_freeConsole(appState->console);
    freeConfig(&appState->options);
    free(appState);
}

void saveApplicationOptions(struct ApplicationState *appState) {
    if (appState == NULL) { return; }

    // Copy the controllers' current bindings into the options. Hotkeys are only on player 1
    struct Controller* controllers[2] = { appState->controller_p1, appState->controller_p2 };
    char name[128];
    for (int player = 0; player < 2; player++) {
        int buttons = (player == 0) ? NUMBER_OF_BUTTONS : NUMBER_OF_NES_BUTTONS;
        for (int b = 0; b < buttons; b++) {
            getBindingAsString(controllers[player], (CONTROLLER_BUTTON)b, name, sizeof(name));
            free(appState->options.bindings[player][b]);
            appState->options.bindings[player][b] = _strdup(name);
        }
    }

    if (saveConfigFile(&appState->options, "config.cfg") != 0) {
        printf("Warning: Could not save the config file.\n");
    }
}

void presentAll(struct ApplicationState *appState) {
    drawFrame(appState->screen);
    drawOverlay(appState->overlay, appState->screen);
    presentFrame(appState->screen);
}

void tickMain(struct ApplicationState *appState) {

    // Pass the events to places that need them. This has to happen every tick, even with no game loaded,
    // or Windows thinks the window has stopped responding and the menus can't be used
    SDL_Event e;
    while (SDL_PollEvent(&e) != 0) {
        handleMenuEvents(e);
        controller_handle_input(appState->controller_p1, e);
        controller_handle_input(appState->controller_p2, e);
    }

    // With no game loaded there's nothing to run. Keep drawing the screen and overlay, at about the NES's frame rate
    if (appState->console->ConnectedCartridge == NULL) {
        presentAll(appState);
        SDL_Delay(16);
        return;
    }

    // Catch a turbo request
    bool turbo = controller_get_pressed(appState->controller_p1, BUTTON_TURBO);

    // Holding rewind pauses the game and shows a strip of thumbnails of the last 30 seconds. Left and right move a cursor
    // along it, frame by frame, and the screen shows that frame. Letting go carries on from there
    bool rewind_held = controller_get_pressed(appState->controller_p1, BUTTON_REWIND);
    if (rewind_held && !appState->rewind->active && !turbo && appState->startup_ready) {
        rewindBegin(appState->rewind, appState->screen, appState->overlay);
    }
    else if (!rewind_held && appState->rewind->active) {
        rewindEnd(appState->rewind, appState->screen, appState->overlay, appState->console);
    }
    if (appState->rewind->active) {
        rewindUpdate(appState->rewind, appState->screen, appState->overlay, appState->console, controller_get_pressed(appState->controller_p1, BUTTON_LEFT), controller_get_pressed(appState->controller_p1, BUTTON_RIGHT));
        turbo = false;
    }

    // Show the fast forward message only while turbo is held: add it when turbo starts, remove it when it stops
    if (turbo && appState->turbo_message_id == OVERLAY_INVALID_ID) {
        appState->turbo_message_id = overlayAddText(appState->overlay, "Fast forwarding...", 8, 8, OVERLAY_NO_EXPIRY);
    }
    else if (!turbo && appState->turbo_message_id != OVERLAY_INVALID_ID) {
        overlayRemove(appState->overlay, appState->turbo_message_id);
        appState->turbo_message_id = OVERLAY_INVALID_ID;
    }

    // Passes the controllers' button states to the controller ports
    NES_setControllerState(appState->console, 0, controller_get_state_as_byte(appState->controller_p1));
    NES_setControllerState(appState->console, 1, controller_get_state_as_byte(appState->controller_p2));

    // Handle startup cycles
    if (!appState->startup_ready) {
        appState->startup_cycles++;
        if (appState->startup_cycles == 29658) { appState->startup_ready = true; }
        return;
    }

    // The game is paused while rewinding. Keep drawing the preview and thumbnails, at about the NES's frame rate
    if (appState->rewind->active) {
        presentAll(appState);
        SDL_Delay(16);
        return;
    }

    // Turbo runs several NES frames for each one shown, then waits once, so the game runs that many times faster
    int frames_to_run = turbo ? TURBO_SPEED : 1;
    for (int f = 0; f < frames_to_run; f++) {
        audio_buffer_count = 0;

        // Run the NES master clock until the PPU has output a full frame
        while (!appState->console->ConnectedPPU->frame_complete) {
            NES_busTickMasterClock(appState->console, appState->startup_ready);
        }
        appState->console->ConnectedPPU->frame_complete = false;

        // Write the game's battery save to disk if it has changed
        NES_tickBatterySave(appState->console->ConnectedCartridge);

        NES_tickHistory(appState->console);
        rewindTickFrame(appState->rewind, appState->screen);
    }

    if (appState->sdlAudioDevice != 0) {
        // Apply the volume. With sound off the samples are made silent rather than skipped, since the queued
        // audio is what paces the emulator
        float volume = appState->options.soundEnabled ? SDL_clamp(appState->options.soundVolume, 0, 100) / 100.0f : 0.0f;
        for (int i = 0; i < audio_buffer_count; i++) { audio_buffer[i] *= volume; }

        // Queue this frame's audio, then wait until the sound card has played enough of it to need the next frame
        SDL_QueueAudio(appState->sdlAudioDevice, audio_buffer, audio_buffer_count * sizeof(float));
        audio_buffer_count = 0;
        while (SDL_GetQueuedAudioSize(appState->sdlAudioDevice) > audio_target_bytes) {
            SDL_Delay(1);
        }
    }
    else {
        // Wait until it's time to show this frame, so the game runs at the NES's real speed
        Uint64 now = SDL_GetPerformanceCounter();
        if (appState->next_frame_time == 0 || now > appState->next_frame_time + appState->frame_ticks) {
            // First frame, or we fell more than a frame behind: resync instead of rushing to catch up
            appState->next_frame_time = now;
        }
        while (now < appState->next_frame_time) {
            // Sleep for most of the wait, then spin for the last millisecond for precision
            Uint64 remaining_ms = (appState->next_frame_time - now) * 1000 / appState->perf_frequency;
            if (remaining_ms > 1) { SDL_Delay((Uint32)(remaining_ms - 1)); }
            now = SDL_GetPerformanceCounter();
        }
        appState->next_frame_time += appState->frame_ticks;
    }

    presentAll(appState);
}

struct ApplicationState* getGlobalApplicationState() {
    return globalAppState;
}