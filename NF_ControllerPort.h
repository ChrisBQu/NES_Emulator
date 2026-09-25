#ifndef NF_CONTROLLER_PORT_H
#define NF_CONTROLLER_PORT_H
#include "NF_Bus.h"
#include <stdint.h>
#include <stdbool.h>

// Games write the strobe to $4016, then read the buttons one bit at a time from $4016 (port 1) and $4017 (port 2)

struct ControllerPort {
	struct NES_Console* bus;
	uint8_t controller_state[2];   // Button state supplied by the front end
	uint8_t controller_shift[2];   // Shift registers the game reads from
	bool controller_strobe;        // While set, the shift registers keep reloading from controller_state
};

// Must be called once to create the ControllerPort object
struct ControllerPort* NF_initControllerPort();

// Shouuld be called once per frame, passing in the state of the buttons
// The state can come from, i.e. reading a gamepad. But it does not come from the emulation layer.
// bit 0 = A, 1 = B, 2 = Select, 3 = Start, 4 = Up, 5 = Down, 6 = Left, 7 = Right
void NF_setControllerState(struct NES_Console* console, int port, uint8_t state);

#endif
