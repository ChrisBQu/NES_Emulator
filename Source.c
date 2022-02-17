#include <SDL.h>
#include <stdio.h>
#include "CF_Window.h"
#include "CF_Controller.h"
#include "NF_Cartridge.h"
#include "NF_6502.h"
#include "NF_Bus.h"
#include "NF_Palette.h"

SDL_Surface* loadedImage = NULL;

bool MAIN = true;
SDL_Event e;

SDL_Surface* screenRenderer;

int scanline;
int cycle;
void receivePixel(struct NF_Pixel pxl) {
		SDL_SetRenderDrawColor(screenRenderer, pxl.r, pxl.g, pxl.b, SDL_ALPHA_OPAQUE);
		SDL_RenderDrawPoint(screenRenderer, pxl.x, pxl.y);
		scanline = pxl.y;
		cycle = pxl.x;
}


uint16_t startup_cycles = 0;
bool startup_ready = false;

void quitFunc() { MAIN = false; }

int main(int arc, char* args[]) {

	uint8_t* rom_data = NF_readROMtoBuffer("DK.nes"); // ...Or, the legally-obtained ROM of your choice. :)
	struct Cartridge *game_cart = NF_createCartridgeFromBuffer(rom_data);
	struct NES_Console *console = NF_initConsole();
	console->imageOutFunc = *receivePixel;

	NF_insertCartridge(console, game_cart);


	CF_init("NES Emulator", 256, 240);

	screenRenderer = SDL_CreateRenderer(CF_getWindow(), -1, SDL_RENDERER_ACCELERATED);

	SDL_Rect slimePos;
	slimePos.x = 20;
	slimePos.y = 20;

	CF_setXFunction(&quitFunc);
	CF_mapButton(SDL_SCANCODE_DOWN, CF_DOWN);
	CF_mapButton(SDL_SCANCODE_A, CF_A);

	while (MAIN) {

		CF_clearControllerInput();

		while (SDL_PollEvent(&e) != NULL) {
			CF_handleXButtonPresses(e);
			CF_receiveControllerInput(e);
		}

		NF_busTickMasterClock(console, startup_ready);

		startup_cycles++;
		if (startup_cycles == 29658 && startup_ready == false) { 
			startup_ready = true;
		}

		if (scanline == 239 && cycle == 255) { 
			SDL_RenderPresent(screenRenderer); 
		}

	}


	CF_exit();

	return 0;

}