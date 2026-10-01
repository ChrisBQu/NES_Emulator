#include <SDL.h>
#include <stdio.h>
#include "ApplicationLayer/Window.h"
#include "ApplicationLayer/Controller.h"
#include "EmulationLayer/Cartridge.h"
#include "EmulationLayer/6502.h"
#include "EmulationLayer/Bus.h"
#include "EmulationLayer/PPU.h"
#include "EmulationLayer/APU.h"
#include "EmulationLayer/Palette.h"
#include "Screen.h"

bool MAIN = true;
SDL_Event e;
int scanline;
int cycle;
uint16_t startup_cycles = 0;
bool startup_ready = false;

struct Screen *gameScreen = NULL;

// Create a rendering function that will plug into the emulator
// Each NES pixel is one framebuffer pixel; the GPU scales the finished frame up to the window's size
void receivePixel(struct NES_Pixel pxl) {
    uint32_t color = 0xFF000000u | ((uint32_t)pxl.r << 16) | ((uint32_t)pxl.g << 8) | pxl.b;
    gameScreen->framebuffer[pxl.y * gameScreen->width + pxl.x] = color;
    scanline = pxl.y;
    cycle = pxl.x;
}

// Audio samples produced by the APU during the current frame; queued to SDL once the frame is done (~735 per frame at 44.1kHz)
#define AUDIO_SAMPLE_RATE 44100
#define AUDIO_BUFFER_SIZE 4096
float audio_buffer[AUDIO_BUFFER_SIZE];
int audio_buffer_count = 0;

// Create an audio function that will plug into the emulator
void receiveSample(float sample) {
    if (audio_buffer_count < AUDIO_BUFFER_SIZE) {
        audio_buffer[audio_buffer_count++] = sample;
    }
}

void quitFunc() { MAIN = false; }



int main(int arc, char* args[]) {

    // Initialize SDL window and renderer
    if (!initWindow("NES Emulator", DEFAULT_SCREEN_WIDTH * DEFAULT_SCREEN_SCALE, DEFAULT_SCREEN_HEIGHT * DEFAULT_SCREEN_SCALE)) { return 1; }


    gameScreen = createScreen(DEFAULT_SCREEN_WIDTH, DEFAULT_SCREEN_HEIGHT);
    if (gameScreen == NULL) { return 1; }

    // Initialize ROM and NES
    uint8_t* rom_data = NES_readROMtoBuffer("CV3.nes"); // Or any other legal ROM.
    if (rom_data == NULL) { return 1; }

    struct Cartridge* game_cart = NES_createCartridgeFromBuffer(rom_data);
    struct NES_Console* console = NES_initConsole();
    struct Controller* controller_p1 = create_controller();
    struct Controller* controller_p2 = create_controller();
    controller_init_default_bindings(controller_p1);

    if (console == 0) { return 1; }
    pairConsoleToWindow(console);
    pairControllersToWindow(controller_p1, controller_p2);

    // Create a function to receive video output from the emulator
    console->imageOutFunc = *receivePixel;

    if (NES_insertCartridge(console, game_cart) == 1) { return 1; }

    // Set what happens when X is pressed on window
    setXFunction(quitFunc);

    // Clear once before the first frame
    clearScreen(gameScreen);

    // Open an audio device
    SDL_AudioDeviceID audio_device = 0;
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) == 0) {
        SDL_AudioSpec want = { 0 };
        want.freq = AUDIO_SAMPLE_RATE;
        want.format = AUDIO_F32SYS;
        want.channels = 1;
        want.samples = 1024;
        want.callback = NULL;
        audio_device = SDL_OpenAudioDevice(NULL, 0, &want, NULL, 0);
    }
    if (audio_device != 0) {
        NES_APU_setSampleRate(console->ConnectedAPU, AUDIO_SAMPLE_RATE);
        console->audioOutFunc = receiveSample;
        SDL_PauseAudioDevice(audio_device, 0);  // Start playback
    }
    else {
        printf("Warning: Could not open an audio device, running without sound. SDL Error: %s\n", SDL_GetError());
    }

    // Start gamepad support. The controllers open their pads when SDL reports them, including pads already plugged in
    if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) != 0) {
        printf("Warning: Could not start gamepad support, only the keyboard will work. SDL Error: %s\n", SDL_GetError());
    }

    // With audio, the emulator is paced by the sound card: it only runs another frame once the queued audio drops below
    // this amount (~50ms). This keeps sound and emulation in sync without pops or growing delay
    const Uint32 audio_target_bytes = (AUDIO_SAMPLE_RATE / 20) * sizeof(float);

    // Without audio, frame pacing falls back to a timer: the NTSC NES outputs ~60.0988 frames per second
    const double NES_FRAMES_PER_SECOND = 60.0988;
    const Uint64 perf_frequency = SDL_GetPerformanceFrequency();
    const Uint64 frame_ticks = (Uint64)(perf_frequency / NES_FRAMES_PER_SECOND);
    Uint64 next_frame_time = 0;

    while (MAIN) {

        // Pass the events to places that need them
        while (SDL_PollEvent(&e) != 0) {
            handleMenuEvents(e);
            controller_handle_input(controller_p1, e);
            controller_handle_input(controller_p2, e);
        }

        // Passes the controllers' button states to the controller ports
        NES_setControllerState(console, 0, controller_get_state_as_byte(controller_p1));
        NES_setControllerState(console, 1, controller_get_state_as_byte(controller_p2));

        // Handle startup cycles
        if (!startup_ready) {
            startup_cycles++;
            if (startup_cycles == 29658) { startup_ready = true; }
            continue;
        }

        // Run the NES master clock until the PPU has output a full frame
        while (!console->ConnectedPPU->frame_complete) {
            NES_busTickMasterClock(console, startup_ready);
        }
        console->ConnectedPPU->frame_complete = false;

        if (audio_device != 0) {
            // Queue this frame's audio, then wait until the sound card has played enough of it to need the next frame
            SDL_QueueAudio(audio_device, audio_buffer, audio_buffer_count * sizeof(float));
            audio_buffer_count = 0;
            while (SDL_GetQueuedAudioSize(audio_device) > audio_target_bytes) {
                SDL_Delay(1);
            }
        }
        else {
            // Wait until it's time to show this frame, so the game runs at the NES's real speed
            Uint64 now = SDL_GetPerformanceCounter();
            if (next_frame_time == 0 || now > next_frame_time + frame_ticks) {
                // First frame, or we fell more than a frame behind: resync instead of rushing to catch up
                next_frame_time = now;
            }
            while (now < next_frame_time) {
                // Sleep for most of the wait, then spin for the last millisecond for precision
                Uint64 remaining_ms = (next_frame_time - now) * 1000 / perf_frequency;
                if (remaining_ms > 1) { SDL_Delay((Uint32)(remaining_ms - 1)); }
                now = SDL_GetPerformanceCounter();
            }
            next_frame_time += frame_ticks;
        }

        presentFrame(gameScreen);
    }

    // Clean up and exit
    if (audio_device != 0) { SDL_CloseAudioDevice(audio_device); }
    destroyScreen(gameScreen);
    exitWindow();

    return 0;
}
