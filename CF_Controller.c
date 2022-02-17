#include "CF_Controller.h"
#include "string.h"
#include <stdio.h>


unsigned short mapped_buttons[255] = { 0 };
bool buttonsPressed[CF_NUMBER_OF_BUTTONS] = { 0 };
bool buttonsHeld[CF_NUMBER_OF_BUTTONS] = { 0 };
bool buttonsReleased[CF_NUMBER_OF_BUTTONS] = { 0 };

void CF_clearControllerInput() {
	memset(buttonsReleased, 0, sizeof(buttonsReleased));
	memset(buttonsPressed, 0, sizeof(buttonsPressed));
}

void CF_receiveControllerInput(SDL_Event e) {
	if (e.type == SDL_KEYDOWN && e.key.repeat==0) {
		buttonsPressed[mapped_buttons[e.key.keysym.scancode]] = true;
		buttonsHeld[mapped_buttons[e.key.keysym.scancode]] = true;
	}
	else if (e.type == SDL_KEYUP) {
		buttonsHeld[mapped_buttons[e.key.keysym.scancode]] = false;
		buttonsReleased[mapped_buttons[e.key.keysym.scancode]] = true;
	}
}

bool* CF_getButtonsPressed() { return buttonsPressed; }
bool* CF_getButtonsHeld() { return buttonsHeld; }
bool* CF_getButtonsReleased() { return buttonsReleased; }

void CF_mapButton(unsigned short from_Scancode, unsigned short to_Button) { mapped_buttons[from_Scancode] = to_Button; }