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

// Can be called once per frame to handle whether or not the user has pressed the X button on the window
void CF_handleXButtonPresses(SDL_Event e);

#endif