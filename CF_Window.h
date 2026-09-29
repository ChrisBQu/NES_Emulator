#ifndef CF_H_WINDOW
#define CF_H_WINDOW

#include <SDL.h>
#include <stdbool.h>

// Call once to create a window at the start of the program running
bool CF_init(const char * screen_name, unsigned short screen_width, unsigned short screen_height);

// Call once before the program is terminated to clear up memory
void CF_exit();

// Get a handle to the window
SDL_Window* CF_getWindow();

// Set the function to be executed if the user presses the X button of the window
void CF_setXFunction(void (*funcPtr)(void));

// Can be called once per frame to handle events where the user interactss with the menu
void CF_handleMenuEvents(SDL_Event e);

// Asssociate a console with the window, so that the menus can access its state
void CF_pairConsoleToWindow(struct NES_Console* console);

#endif