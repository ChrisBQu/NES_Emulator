#ifndef WINDOW_H
#define WINDOW_H

#include <SDL.h>
#include <stdbool.h>

// Declared here so the functions below can take pointers to them without including their headers
struct NES_Console;
struct Controller;
struct Overlay;
struct Screen;

// Call once to create a window at the start of the program running
bool initWindow(const char * screen_name, unsigned short screen_width, unsigned short screen_height);

// Call once before the program is terminated to clear up memory
void exitWindow();

// Get a handle to the window
SDL_Window* getWindow();

// Set the function to be executed if the user presses the X button of the window
void setXFunction(void (*funcPtr)(void));

// Set the menu items' check marks to match the application state's options. The menus are built before the
// application state exists, so call this once it's been created
void updateMenusFromApplicationState();

// Turn the CRT filter on or off, and update the menu's check mark to match. Doesn't save the config file
void setCrtFilterEnabled(bool enabled);

// Can be called once per frame to handle events where the user interactss with the menu
void handleMenuEvents(SDL_Event e);

#endif