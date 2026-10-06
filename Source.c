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
#include "EmulationLayer/BatterySave.h"
#include "EmulationLayer/Savestate.h"
#include "ApplicationLayer/Overlay.h"
#include "ApplicationLayer/Rewind.h"
#include "ApplicationLayer/Application.h"
#include "ApplicationLayer/Config.h"

#define APPLICATION_NAME "Wizzrobe"

struct ApplicationState *appState;

bool MAIN = true;

// Create a function for when the window is closed
void quitFunc() { MAIN = false; }

int main(int arc, char* args[]) {

    // Initialize SDL window. This must come before the application state, as the screen creates its OpenGL context on the window.
    // It's resized to the scale from the options once they're loaded
    if (!initWindow(APPLICATION_NAME, DEFAULT_SCREEN_WIDTH, DEFAULT_SCREEN_HEIGHT)) { return 1; }

    // Initialize the application state
    appState = initApplicationState();
    if (appState == NULL) { return 1; }
    updateMenusFromApplicationState();
    
    // Set what happens when X is pressed on window
    setXFunction(quitFunc);

    while (MAIN) {
        tickMain(appState);
    }

    // Clean up and exit
    destroyApplicationState(appState);
    exitWindow();

    return 0;
}
