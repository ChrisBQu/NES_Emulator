#include "CF_Window.h"
#include <stdio.h>

SDL_Window* window = NULL;

bool CF_init(const char * screen_name, unsigned short screen_w, unsigned short screen_h) {
	// Attempt to initialize SDL, and bail if it doesn't work
	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		printf("SDL could not be initialized.");
		return false;
	}
	// Attempt to create the window, and bail if it doesn't work
	window = SDL_CreateWindow(screen_name, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, screen_w, screen_h, SDL_WINDOW_SHOWN);
	if (window == NULL) {
		printf("Error: Window could not be created.");
		return false;
	}
	return true;
}

SDL_Window* CF_getWindow() {
	return window;
}

void CF_exit() {
	SDL_DestroyWindow(window);
	SDL_Quit();
}


void (*XFunctionPtr)(void) = NULL;
void CF_setXFunction(void (*funcPtr)(void)) { XFunctionPtr = funcPtr; }

void CF_handleXButtonPresses(SDL_Event e) {
	if (e.type == SDL_QUIT) { (*XFunctionPtr)(); }
}