#include "CF_Controller.h"
#include "string.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>


#define CONTROL_STICK_PRESS_THRESHOLD 8000
#define MAX_NUMBER_OF_CONTROLLER_BUTTONS 64

typedef struct ControlStickMovement {
	int stick;
	int axis;
	int direction;
} ControlStickMovement;

// Helper function for the control stick hash function
static uint64_t mix(uint64_t x) {
	x ^= x >> 33;
	x *= 0xff51afd7ed558ccdULL;
	x ^= x >> 33;
	return x;
}

// Hash function to allow us to use ControlStickMovement as a key in the hashmap
static uint64_t control_stick_movement_hash(const void* key) {
	const struct ControlStickMovement* k = key;
	uint64_t h = mix((uint32_t)k->stick);
	h = mix(h ^ (uint32_t)k->axis);
	h = mix(h ^ (uint32_t)k->direction);
	return h;
}

// Equality function to allow us to use ControlStickMovement as a key in the hashmap
static int control_stick_movement_eq(const void* a, const void* b) {
	const struct ControlStickMovement* x = a;
	const struct ControlStickMovement* y = b;
	return (x->stick == y->stick && x->axis == y->axis && x->direction == y->direction);
}

// Hash function to allow us to use SDL_Keycode as a key in the hashmap
static uint64_t keycode_hash(const void* key) {
	uint64_t x = (uint32_t) * (const SDL_Keycode*)key;
	x ^= x >> 33;
	x *= 0xff51afd7ed558ccdULL;
	x ^= x >> 33;
	return x;
}

// Equality function to allow us to use SDL_Keycode as a key in the hashmap
static int keycode_eq(const void* a, const void* b) {
	return *(const SDL_Keycode*)a == *(const SDL_Keycode*)b;
}

struct Controller {
	Hashmap* bindings;
	Hashmap* stickBindings;
	bool buttons_pressed[MAX_NUMBER_OF_CONTROLLER_BUTTONS];
	SDL_GameControllerButton button_ids[MAX_NUMBER_OF_CONTROLLER_BUTTONS]; // Storage the hashmap values point into
	SDL_GameController* internalController;
};

// Constructor function for a controller
Controller* create_controller() {
	Controller* controller = calloc(1, sizeof(Controller));
	if (controller == NULL) { return NULL; }

	bool failed_allocation = false;
	controller->bindings = create_hashmap(128, sizeof(SDL_Keycode), keycode_hash, keycode_eq);
	if (controller->bindings == NULL) { failed_allocation = true; }
	controller->stickBindings = create_hashmap(128, sizeof(struct ControlStickMovement), control_stick_movement_hash, control_stick_movement_eq);
	if (controller->stickBindings == NULL) { failed_allocation = true; }

	if (failed_allocation) {
		if (controller->bindings != NULL) { hashmap_destroy(controller->bindings); }
		if (controller->stickBindings != NULL) { hashmap_destroy(controller->stickBindings); }
		free(controller);
		return NULL;
	}

	for (int i = 0; i < MAX_NUMBER_OF_CONTROLLER_BUTTONS; i++) { controller->button_ids[i] = (SDL_GameControllerButton)i; }
	controller->internalController = NULL;
	return controller;
}

// Destructor function for a controller
void destroy_controller(Controller* controller) {
	if (controller == NULL) { return; }
	hashmap_destroy(controller->bindings);
	hashmap_destroy(controller->stickBindings);
	if (controller->internalController != NULL) { SDL_GameControllerClose(controller->internalController); }
	free(controller);
}

// Bind a keyboarrd key to an SDL button
void controller_bind_key_to_button(Controller* controller, SDL_Keycode key, CF_BUTTON button) {
	if (controller == NULL || button < 0 || button >= MAX_NUMBER_OF_CONTROLLER_BUTTONS) { return; }
	hashmap_set(controller->bindings, &key, &controller->button_ids[button]);
}

// Bind a control stick movement to an SDL button
void controller_bind_stick_to_button(Controller* controller, int from_stick, int from_axis, int from_direction, SDL_GameControllerButton to_button) {
	if (controller == NULL || to_button < 0 || to_button >= MAX_NUMBER_OF_CONTROLLER_BUTTONS) { return; }
	ControlStickMovement movement = { from_stick, from_axis, from_direction };
	hashmap_set(controller->stickBindings, &movement, &controller->button_ids[to_button]);
}

// Assigns default bindings for a controller
void controller_init_default_bindings(Controller* controller) {
	controller_bind_key_to_button(controller, SDLK_a, SDL_CONTROLLER_BUTTON_Y);
	controller_bind_key_to_button(controller, SDLK_s, SDL_CONTROLLER_BUTTON_X);
	controller_bind_key_to_button(controller, SDLK_s, SDL_CONTROLLER_BUTTON_X);
	controller_bind_key_to_button(controller, SDLK_d, SDL_CONTROLLER_BUTTON_B);
	controller_bind_key_to_button(controller, SDLK_f, SDL_CONTROLLER_BUTTON_A);
	controller_bind_key_to_button(controller, SDLK_z, SDL_CONTROLLER_BUTTON_LEFTSHOULDER);
	controller_bind_key_to_button(controller, SDLK_x, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);
	controller_bind_key_to_button(controller, SDLK_RETURN, SDL_CONTROLLER_BUTTON_START);
	controller_bind_key_to_button(controller, SDLK_LSHIFT, SDL_CONTROLLER_BUTTON_BACK);
	controller_bind_key_to_button(controller, SDLK_UP, SDL_CONTROLLER_BUTTON_DPAD_UP);
	controller_bind_key_to_button(controller, SDLK_DOWN, SDL_CONTROLLER_BUTTON_DPAD_DOWN);
	controller_bind_key_to_button(controller, SDLK_LEFT, SDL_CONTROLLER_BUTTON_DPAD_LEFT);
	controller_bind_key_to_button(controller, SDLK_RIGHT, SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
}

// Call once per frame for each controller to handle button presses and update the state accordingly
void controller_handle_input(Controller* controller, SDL_Event ev) {
	if (controller == NULL) { return; }

	// Keyboard key pressed
	if (ev.type == SDL_KEYDOWN) {
		SDL_GameControllerButton* btn = hashmap_get(controller->bindings, &ev.key.keysym.sym);
		if (btn != NULL) { controller->buttons_pressed[*btn] = true; }
	}

	// Keyboard key released
	else if (ev.type == SDL_KEYUP) {
		SDL_GameControllerButton* btn = hashmap_get(controller->bindings, &ev.key.keysym.sym);
		if (btn != NULL) { controller->buttons_pressed[*btn] = false; }
	}

	// Controller button pressed
	else if (ev.type == SDL_CONTROLLERBUTTONDOWN) {
		SDL_GameControllerButton btn = ev.cbutton.button;
		controller->buttons_pressed[btn] = true;
	}

	// Controller button released
	else if (ev.type == SDL_CONTROLLERBUTTONUP) {
		SDL_GameControllerButton btn = ev.cbutton.button;
		controller->buttons_pressed[btn] = false;
	}

	// Control stick to D-Pad mapping
	else if (ev.type == SDL_CONTROLLERAXISMOTION) {
		int value = ev.caxis.value;
		int stick = ev.caxis.which;
		int axis = ev.caxis.axis;
		int direction = (value < 0) ? -1 : 1;
		if (abs(value) >= CONTROL_STICK_PRESS_THRESHOLD) {
			ControlStickMovement movement = { stick, axis, direction };
			SDL_GameControllerButton* btn = hashmap_get(controller->stickBindings, &movement);
			if (btn != NULL) { controller->buttons_pressed[*btn] = true; }
		}
		else {
			// The stick returned to center, so release whatever was bouunud to either direection
			ControlStickMovement negative = { stick, axis, -1 };
			ControlStickMovement positive = { stick, axis, 1 };
			SDL_GameControllerButton* btn = hashmap_get(controller->stickBindings, &negative);
			if (btn != NULL) { controller->buttons_pressed[*btn] = false; }
			btn = hashmap_get(controller->stickBindings, &positive);
			if (btn != NULL) { controller->buttons_pressed[*btn] = false; }
		}
	}
}

bool controller_get_pressed(Controller* controller, SDL_GameControllerButton b) {
	if (b < 0 || b >= MAX_NUMBER_OF_CONTROLLER_BUTTONS) { return false; }
	return controller->buttons_pressed[b];
}

uint8_t controller_get_state_as_byte(Controller* controller) {
	if (controller == NULL) { return 0; }

	// The SDL button that drives each NES bit, in the order the NES shifts them out
	static const SDL_GameControllerButton nes_order[8] = {
		SDL_CONTROLLER_BUTTON_A,
		SDL_CONTROLLER_BUTTON_B,
		SDL_CONTROLLER_BUTTON_BACK,
		SDL_CONTROLLER_BUTTON_START,
		SDL_CONTROLLER_BUTTON_DPAD_UP,
		SDL_CONTROLLER_BUTTON_DPAD_DOWN,
		SDL_CONTROLLER_BUTTON_DPAD_LEFT,
		SDL_CONTROLLER_BUTTON_DPAD_RIGHT
	};

	uint8_t state = 0;
	for (int i = 0; i < 8; i++) {
		if (controller->buttons_pressed[nes_order[i]]) { state |= (uint8_t)(1 << i); }
	}

	// Opposite D-pad directions should not be preessable at the same time
	if ((state & 0x30) == 0x30) { state &= ~0x30; }
	if ((state & 0xC0) == 0xC0) { state &= ~0xC0; }
	return state;
}