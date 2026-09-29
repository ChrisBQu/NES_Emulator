#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <SDL.h>
#include <stdbool.h>
#include <stdint.h>

#include "../Utils/Hashmap.h"

typedef enum { BUTTON_UP, BUTTON_RIGHT, BUTTON_DOWN, BUTTON_LEFT, BUTTON_A, BUTTON_B, BUTTON_SELECT, BUTTON_START, NUMBER_OF_BUTTONS } CONTROLLER_BUTTON;

typedef struct Controller Controller;

Controller* create_controller();
void destroy_controller(Controller *controller);

void controller_init_default_bindings(Controller *controller);

void controller_bind_key_to_button(Controller* controller, SDL_Keycode key, CONTROLLER_BUTTON button);
void controller_bind_stick_to_button(Controller *controller, int from_stick, int from_axis, int from_direction, SDL_GameControllerButton to_button);
void controller_handle_input(Controller *controller, SDL_Event event);

bool controller_get_pressed(Controller* controller, SDL_GameControllerButton b);

// Pack the button state into the byte the NES controller shift register is loaded with
// bit 0 = A, 1 = B, 2 = Select, 3 = Start, 4 = Up, 5 = Down, 6 = Left, 7 = Right
uint8_t controller_get_state_as_byte(Controller* controller);

#endif
