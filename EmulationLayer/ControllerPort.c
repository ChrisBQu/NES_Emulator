#include "ControllerPort.h"
#include "Bus.h"
#include <stdio.h>
#include <stdlib.h>

// Constructor
struct ControllerPort* NES_initControllerPort() {
	struct ControllerPort* port = calloc(1, sizeof(struct ControllerPort));  // Zeroed: no buttons held, strobe off
	if (port == NULL) {
		printf("Error: Could not create ControllerPort object. Out of memory?\n");
		return NULL;
	}
	return port;
}

void NES_setControllerState(struct NES_Console* console, int port, uint8_t state) {
	if (port < 0 || port > 1) { return; }
	console->ConnectedControllerPort->controller_state[port] = state;
}