#include <SDL.h>
#include <stdio.h>
#include "CF_Window.h"
#include "NF_Cartridge.h"
#include "NF_6502.h"
#include "NF_Bus.h"
#include "NF_Palette.h"

bool MAIN = true;
SDL_Event e;

// Change this to SDL_Renderer* for proper SDL rendering
SDL_Renderer* screenRenderer;

int scanline;
int cycle;
uint16_t startup_cycles = 0;
bool startup_ready = false;

// Create a rendering function that will plug into the emulator
void receivePixel(struct NF_Pixel pxl) {
    SDL_SetRenderDrawColor(screenRenderer, pxl.r, pxl.g, pxl.b, SDL_ALPHA_OPAQUE);
    SDL_RenderDrawPoint(screenRenderer, pxl.x, pxl.y);
    scanline = pxl.y;
    cycle = pxl.x;
}

void quitFunc() { MAIN = false; }

int main(int arc, char* args[]) {

    // Initialize ROM and NES
    uint8_t* rom_data = NF_readROMtoBuffer("nestest.nes"); // Or any other legal ROM.
    struct Cartridge* game_cart = NF_createCartridgeFromBuffer(rom_data);
    struct NES_Console* console = NF_initConsole();
    
    if (console == 0) { return 1; }

    console->imageOutFunc = *receivePixel;

    if (NF_insertCartridge(console, game_cart) == 1) { return 1; }

    // Initialize SDL window and renderer
    CF_init("NES Emulator", 256, 240);

    // Correctly create the renderer
    int v = SDL_Init(SDL_INIT_VIDEO);
    screenRenderer = SDL_CreateRenderer(CF_getWindow(), -1, SDL_RENDERER_ACCELERATED);
    if (screenRenderer == NULL) {
        printf("Renderer could not be created! SDL Error: %s\n", SDL_GetError());
        return -1;
    }
    if (!screenRenderer) {
        printf("Error: SDL_Renderer could not be created! SDL Error: %s\n", SDL_GetError());
        return -1;
    }

    // Set what happens when X is pressed on window
    CF_setXFunction(quitFunc);

    while (MAIN) {

        // Look for window closing
        while (SDL_PollEvent(&e) != NULL) { CF_handleXButtonPresses(e); }

        // Clear the screen at the start of each frame
        SDL_SetRenderDrawColor(screenRenderer, 0, 0, 0, SDL_ALPHA_OPAQUE);  // Clear with black

        // Handle startup cycles
        startup_cycles++;
        if (startup_cycles == 29658 && startup_ready == false) {
            startup_ready = true;
        }

        // Run the NES master clock
        if (startup_ready) {
            NF_busTickMasterClock(console, startup_ready);
        }

        // Update the screen when the frame ends
        if (scanline == 239 && cycle == 254) {
            SDL_RenderPresent(screenRenderer);
        }
    }

    // Clean up and exit
    CF_exit();

    return 0;
}
