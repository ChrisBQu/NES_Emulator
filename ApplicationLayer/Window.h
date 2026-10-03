#ifndef WINDOW_H
#define WINDOW_H

#include <SDL.h>
#include <stdbool.h>

// Declared here so the functions below can take pointers to them without including their headers
struct NES_Console;
struct Controller;
struct Overlay;

// Call once to create a window at the start of the program running
bool initWindow(const char * screen_name, unsigned short screen_width, unsigned short screen_height);

// Call once before the program is terminated to clear up memory
void exitWindow();

// Get a handle to the window
SDL_Window* getWindow();

// Set the function to be executed if the user presses the X button of the window
void setXFunction(void (*funcPtr)(void));

// Can be called once per frame to handle events where the user interactss with the menu
void handleMenuEvents(SDL_Event e);

// Asssociate a console with the window, so that the menus can access its state
void pairConsoleToWindow(struct NES_Console* console);

// Associate the two players' controllers with the window, so the controller dialog can show and change their bindings
void pairControllersToWindow(struct Controller* player1, struct Controller* player2);

// Associate an overlay with the window, so the menus can show messages over the game
void pairOverlayToWindow(struct Overlay* overlay);

#endif