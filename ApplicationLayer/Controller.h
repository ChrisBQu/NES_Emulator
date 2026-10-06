#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <SDL.h>
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    BUTTON_A,
    BUTTON_B,
    BUTTON_SELECT,
    BUTTON_START,
    BUTTON_UP,
    BUTTON_DOWN,
    BUTTON_LEFT,
    BUTTON_RIGHT,
    NUMBER_OF_NES_BUTTONS,
    // Emulator hotkeys. These aren't sent to the NES, and are only used on player 1's controller
    BUTTON_REWIND = NUMBER_OF_NES_BUTTONS,
    BUTTON_TURBO,
    NUMBER_OF_BUTTONS } CONTROLLER_BUTTON;

typedef struct Controller Controller;

Controller* create_controller();
void destroy_controller(Controller *controller);

void controller_init_default_bindings(Controller *controller);

// Each NES button has at most one binding: a key, a gamepad button, or a stick direction. Binding replaces whatever
// the NES button had before. The same input can be bound to more than one NES button
void controller_bind_key_to_button(Controller* controller, SDL_Keycode key, CONTROLLER_BUTTON button);
void controller_bind_pad_button_to_button(Controller* controller, SDL_GameControllerButton pad_button, CONTROLLER_BUTTON button);
void controller_bind_stick_to_button(Controller *controller, SDL_GameControllerAxis axis, int direction, CONTROLLER_BUTTON button);

// Look at the controller's own gamepad right now, and bind the first pressed button, or stick pushed past the
// threshold, to an NES button. Returns false if nothing was pressed. Call SDL_GameControllerUpdate first if the
// SDL event loop isn't running, so the pad's state is current. This exists because the control configuration dialog
// needs a way to inject bindings.
bool controller_bind_from_pad_state(Controller* controller, CONTROLLER_BUTTON button);

// Remove an NES button's binding
void clearBindings(Controller* controller, CONTROLLER_BUTTON b);

// Write the name of what's bound to an NES button into output (e.g. "F", "dpup" or "leftx+"), or "<Unmapped>" if nothing is.
// Returns output, so it can be used directly as an argument
char* getBindingAsString(Controller* controller, CONTROLLER_BUTTON b, char* output, size_t output_len);

// The reverse of getBindingAsString: bind whatever the name describes to an NES button. "<Unmapped>" clears the binding.
// Lowercase gamepad button names ("a") are read as gamepad buttons, so a key needs its key name ("A").
// Returns false if the name isn't recognised, leaving the binding as it was
bool controller_bind_from_string(Controller* controller, const char* name, CONTROLLER_BUTTON b);
void controller_handle_input(Controller *controller, SDL_Event event);

bool controller_get_pressed(Controller* controller, CONTROLLER_BUTTON b);

// Pack the button state into the byte the NES controller shift register is loaded with
// bit 0 = A, 1 = B, 2 = Select, 3 = Start, 4 = Up, 5 = Down, 6 = Left, 7 = Right
uint8_t controller_get_state_as_byte(Controller* controller);

#endif
