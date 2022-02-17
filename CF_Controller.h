#ifndef CF_H_CONTROLLER
#define CF_H_CONTROLLER
#include <SDL.h>
#include <stdbool.h>

// An array to represent the virtual control pad 
typedef enum { CF_UP, CF_RIGHT, CF_DOWN, CF_LEFT, CF_A, CF_B, CF_X, CF_Y, CF_SELECT, CF_START, CF_NUMBER_OF_BUTTONS } CF_BUTTON;

// The following three functions return
bool* CF_getButtonsPressed();
bool* CF_getButtonsHeld();
bool* CF_getButtonsReleased();

// Map an SDL Scancode to one of the buttons on the virtual controller
void CF_mapButton(unsigned short from_Scancode, CF_BUTTON to_Button);

// Call once per frame, at the beginning of the frame, to clear flags for pressed and released keys
void CF_clearControllerInput();

// All key presses and releases should be passed into this function once per frame
// This should be called after CF_clearControllerInput() but before attempting to read the state of the button arrays
void CF_receiveControllerInput(SDL_Event e);

#endif
