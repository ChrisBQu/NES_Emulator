#include "Controller.h"
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>


#define CONTROL_STICK_PRESS_THRESHOLD 8000

typedef enum { BINDING_NONE, BINDING_KEY, BINDING_PAD_BUTTON, BINDING_STICK } BINDING_TYPE;

// The one input that presses an NES button. The same input can be bound to several NES buttons
typedef struct Binding {
	BINDING_TYPE type;
	union {                                    // Only the member matching type is used
		SDL_Keycode key;
		SDL_GameControllerButton pad_button;
		struct {
			SDL_GameControllerAxis axis;
			int direction;                     // -1 or 1
		} stick;
	};
} Binding;

struct Controller {
	Binding bindings[NUMBER_OF_BUTTONS];       // Indexed by CONTROLLER_BUTTON
	bool buttons_pressed[NUMBER_OF_BUTTONS];
	SDL_GameController* internalController;
};

// Helper function: whether a gamepad event came from this controller's own pad, so one pad doesn't drive both players
static bool isOwnPad(Controller* controller, SDL_JoystickID which) {
	if (controller->internalController == NULL) { return false; }
	return which == SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controller->internalController));
}

// Helper function: replace an NES button's binding
static void setBinding(Controller* controller, CONTROLLER_BUTTON button, Binding binding) {
	if (controller == NULL || button < 0 || button >= NUMBER_OF_BUTTONS) { return; }
	controller->bindings[button] = binding;
	// If it was held, the old input's release can't reach it any more, so release it now
	controller->buttons_pressed[button] = false;
}

// Constructor function for a controller
Controller* create_controller() {
	// calloc zeroes everything, so every button starts as BINDING_NONE and not pressed
	Controller* controller = calloc(1, sizeof(Controller));
	if (controller == NULL) { return NULL; }
	controller->internalController = NULL;
	return controller;
}

// Destructor function for a controller
void destroy_controller(Controller* controller) {
	if (controller == NULL) { return; }
	if (controller->internalController != NULL) { SDL_GameControllerClose(controller->internalController); }
	free(controller);
}

// Bind a keyboarrd key to an NES button
void controller_bind_key_to_button(Controller* controller, SDL_Keycode key, CONTROLLER_BUTTON button) {
	Binding binding = { BINDING_KEY };
	binding.key = key;
	setBinding(controller, button, binding);
}

// Bind a gamepad button to an NES button
void controller_bind_pad_button_to_button(Controller* controller, SDL_GameControllerButton pad_button, CONTROLLER_BUTTON button) {
	if (pad_button < 0 || pad_button >= SDL_CONTROLLER_BUTTON_MAX) { return; }
	Binding binding = { BINDING_PAD_BUTTON };
	binding.pad_button = pad_button;
	setBinding(controller, button, binding);
}

// Bind a control stick movement to an NES button
void controller_bind_stick_to_button(Controller* controller, SDL_GameControllerAxis axis, int direction, CONTROLLER_BUTTON button) {
	if (axis < 0 || axis >= SDL_CONTROLLER_AXIS_MAX) { return; }
	Binding binding = { BINDING_STICK };
	binding.stick.axis = axis;
	binding.stick.direction = (direction < 0) ? -1 : 1;
	setBinding(controller, button, binding);
}

bool controller_bind_from_pad_state(Controller* controller, CONTROLLER_BUTTON button) {
	if (controller == NULL || controller->internalController == NULL) { return false; }
	SDL_GameController* pad = controller->internalController;

	for (int i = 0; i < SDL_CONTROLLER_BUTTON_MAX; i++) {
		if (SDL_GameControllerGetButton(pad, (SDL_GameControllerButton)i)) {
			controller_bind_pad_button_to_button(controller, (SDL_GameControllerButton)i, button);
			return true;
		}
	}
	for (int axis = 0; axis < SDL_CONTROLLER_AXIS_MAX; axis++) {
		int value = SDL_GameControllerGetAxis(pad, (SDL_GameControllerAxis)axis);
		if (abs(value) >= CONTROL_STICK_PRESS_THRESHOLD) {
			controller_bind_stick_to_button(controller, (SDL_GameControllerAxis)axis, (value < 0) ? -1 : 1, button);
			return true;
		}
	}
	return false;
}

void clearBindings(Controller* controller, CONTROLLER_BUTTON b) {
	Binding binding = { BINDING_NONE };
	setBinding(controller, b, binding);
}

char* getBindingAsString(Controller* controller, CONTROLLER_BUTTON b, char* output, size_t output_len) {
	if (output == NULL || output_len == 0) { return output; }
	if (controller == NULL || b < 0 || b >= NUMBER_OF_BUTTONS) {
		snprintf(output, output_len, "<Unmapped>");
		return output;
	}

	const Binding* binding = &controller->bindings[b];
	switch (binding->type) {
		case BINDING_KEY:
			snprintf(output, output_len, "%s", SDL_GetKeyName(binding->key));
			break;
		case BINDING_PAD_BUTTON: {
			const char* name = SDL_GameControllerGetStringForButton(binding->pad_button);
			if (name != NULL) { snprintf(output, output_len, "%s", name); }
			else { snprintf(output, output_len, "Button %d", binding->pad_button); }
			break;
		}
		case BINDING_STICK: {
			// e.g. "leftx+" for the left stick pushed right
			const char* name = SDL_GameControllerGetStringForAxis(binding->stick.axis);
			snprintf(output, output_len, "%s%c", (name != NULL) ? name : "axis", (binding->stick.direction < 0) ? '-' : '+');
			break;
		}
		default:
			snprintf(output, output_len, "<Unmapped>");
			break;
	}
	return output;
}

bool controller_bind_from_string(Controller* controller, const char* name, CONTROLLER_BUTTON b) {
	if (controller == NULL || name == NULL) { return false; }

	if (strcmp(name, "<Unmapped>") == 0) {
		clearBindings(controller, b);
		return true;
	}

	// Gamepad button names are lowercase ("a", "dpup"), and compared exactly so they aren't mistaken for key names ("A")
	for (int i = 0; i < SDL_CONTROLLER_BUTTON_MAX; i++) {
		const char* padName = SDL_GameControllerGetStringForButton((SDL_GameControllerButton)i);
		if (padName != NULL && strcmp(name, padName) == 0) {
			controller_bind_pad_button_to_button(controller, (SDL_GameControllerButton)i, b);
			return true;
		}
	}

	// A stick direction is the axis name followed by + or - (e.g. "leftx+")
	size_t length = strlen(name);
	if (length >= 2 && (name[length - 1] == '+' || name[length - 1] == '-')) {
		for (int axis = 0; axis < SDL_CONTROLLER_AXIS_MAX; axis++) {
			const char* axisName = SDL_GameControllerGetStringForAxis((SDL_GameControllerAxis)axis);
			if (axisName != NULL && strlen(axisName) == length - 1 && strncmp(name, axisName, length - 1) == 0) {
				controller_bind_stick_to_button(controller, (SDL_GameControllerAxis)axis, (name[length - 1] == '-') ? -1 : 1, b);
				return true;
			}
		}
	}

	// Otherwise it should be a key name
	SDL_Keycode key = SDL_GetKeyFromName(name);
	if (key == SDLK_UNKNOWN) { return false; }
	controller_bind_key_to_button(controller, key, b);
	return true;
}

// Assigns default bindings for a controller
void controller_init_default_bindings(Controller* controller) {
	controller_bind_key_to_button(controller, SDLK_d, BUTTON_B);
	controller_bind_key_to_button(controller, SDLK_f, BUTTON_A);
	controller_bind_key_to_button(controller, SDLK_RETURN, BUTTON_START);
	controller_bind_key_to_button(controller, SDLK_LSHIFT, BUTTON_SELECT);
	controller_bind_key_to_button(controller, SDLK_UP, BUTTON_UP);
	controller_bind_key_to_button(controller, SDLK_DOWN, BUTTON_DOWN);
	controller_bind_key_to_button(controller, SDLK_LEFT, BUTTON_LEFT);
	controller_bind_key_to_button(controller, SDLK_RIGHT, BUTTON_RIGHT);
	controller_bind_key_to_button(controller, SDLK_BACKSPACE, BUTTON_REWIND);
	controller_bind_key_to_button(controller, SDLK_TAB, BUTTON_TURBO);
}

// Call once per frame for each controller to handle button presses and update the state accordingly
void controller_handle_input(Controller* controller, SDL_Event ev) {
	if (controller == NULL) { return; }

	// A gamepad was plugged in. SDL also sends this at startup for pads that are already connected.
	// Each controller takes the first pad that isn't open yet, so the first pad goes to player 1 and the second to player 2
	if (ev.type == SDL_CONTROLLERDEVICEADDED) {
		if (controller->internalController != NULL) { return; }
		// For this event, which is a device index rather than an instance ID
		if (SDL_GameControllerFromInstanceID(SDL_JoystickGetDeviceInstanceID(ev.cdevice.which)) != NULL) { return; } // The other player has it
		controller->internalController = SDL_GameControllerOpen(ev.cdevice.which);
		if (controller->internalController == NULL) { printf("Warning: Could not open gamepad. SDL Error: %s\n", SDL_GetError()); }
		return;
	}

	// A gamepad was unplugged
	if (ev.type == SDL_CONTROLLERDEVICEREMOVED) {
		if (!isOwnPad(controller, ev.cdevice.which)) { return; }
		SDL_GameControllerClose(controller->internalController);
		controller->internalController = NULL;
		// The release events for anything that was held will never arrive, so let go of everything
		memset(controller->buttons_pressed, 0, sizeof(controller->buttons_pressed));
		return;
	}

	// Keyboard key pressed or released
	if (ev.type == SDL_KEYDOWN || ev.type == SDL_KEYUP) {
		for (int b = 0; b < NUMBER_OF_BUTTONS; b++) {
			if (controller->bindings[b].type == BINDING_KEY && controller->bindings[b].key == ev.key.keysym.sym) {
				controller->buttons_pressed[b] = (ev.type == SDL_KEYDOWN);
			}
		}
	}

	// Gamepad button pressed or released
	else if ((ev.type == SDL_CONTROLLERBUTTONDOWN || ev.type == SDL_CONTROLLERBUTTONUP) && isOwnPad(controller, ev.cbutton.which)) {
		for (int b = 0; b < NUMBER_OF_BUTTONS; b++) {
			if (controller->bindings[b].type == BINDING_PAD_BUTTON && controller->bindings[b].pad_button == ev.cbutton.button) {
				controller->buttons_pressed[b] = (ev.type == SDL_CONTROLLERBUTTONDOWN);
			}
		}
	}

	// Control stick moved. Each NES button bound to this axis is held while the stick is pushed far enough its way
	else if (ev.type == SDL_CONTROLLERAXISMOTION && isOwnPad(controller, ev.caxis.which)) {
		int value = ev.caxis.value;
		for (int b = 0; b < NUMBER_OF_BUTTONS; b++) {
			const Binding* binding = &controller->bindings[b];
			if (binding->type == BINDING_STICK && binding->stick.axis == ev.caxis.axis) {
				controller->buttons_pressed[b] = (value * binding->stick.direction >= CONTROL_STICK_PRESS_THRESHOLD);
			}
		}
	}
}

bool controller_get_pressed(Controller* controller, CONTROLLER_BUTTON b) {
	if (controller == NULL || b < 0 || b >= NUMBER_OF_BUTTONS) { return false; }
	return controller->buttons_pressed[b];
}

uint8_t controller_get_state_as_byte(Controller* controller) {
	if (controller == NULL) { return 0; }

	// CONTROLLER_BUTTON is in the order the NES shifts the buttons out, so each button's value is its bit
	uint8_t state = 0;
	for (int i = 0; i < NUMBER_OF_NES_BUTTONS; i++) {
		if (controller->buttons_pressed[i]) { state |= (uint8_t)(1 << i); }
	}

	// Opposite D-pad directions should not be preessable at the same time
	if ((state & 0x30) == 0x30) { state &= ~0x30; }
	if ((state & 0xC0) == 0xC0) { state &= ~0xC0; }
	return state;
}