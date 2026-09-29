#include "Window.h"
#include "../EmulationLayer/Bus.h"
#include "../EmulationLayer/Cartridge.h"
#include <stdio.h>
#include <SDL.h>
#include <SDL_syswm.h>
#include <windows.h>
#include <commdlg.h>

#define MAX_ROM_PATH_LENGTH 1024

static SDL_Window* window = NULL;
static HMENU menuBar = NULL;
static struct NES_Console* pairedConsole = NULL;

enum MENU_IDS {
	MENU_ID_LOAD_ROM = 1001,
	MENU_ID_FILE_EXIT,
};

// Helper function to get the HWND from the main SDL window
static HWND getHWND() {
	SDL_SysWMinfo info;
	SDL_VERSION(&info.version);
	if (SDL_GetWindowWMInfo(window, &info) == 0) { return NULL; }
	return info.info.win.window;
}

// Helper function to attach menus to the window
static void initMenus() {
	HWND hwnd = getHWND();
	if (hwnd == NULL) { return; }
	
	menuBar = CreateMenu();
	HMENU fileMenu = CreatePopupMenu();
	AppendMenuA(fileMenu, MF_STRING, MENU_ID_LOAD_ROM, "Load ROM...");
	AppendMenuA(fileMenu, MF_SEPARATOR, 0, NULL);
	AppendMenuA(fileMenu, MF_STRING, MENU_ID_FILE_EXIT, "Exit");
	AppendMenuA(menuBar, MF_POPUP, (UINT_PTR)fileMenu, "File");

	SetMenu(hwnd, menuBar);
    int w, h;
    SDL_GetWindowSize(window, &w, &h);
    SDL_SetWindowSize(window, w, h);
    SDL_EventState(SDL_SYSWMEVENT, SDL_ENABLE);

}

bool initWindow(const char *screen_name, unsigned short screen_w, unsigned short screen_h) {
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

	initMenus();

	return true;
}

SDL_Window* getWindow() {
	return window;
}

void exitWindow() {
	SDL_DestroyWindow(window);
	SDL_Quit();
}

static int loadROMDialog(HWND owner, char *output, DWORD output_len) {
	OPENFILENAMEA filename = {0};
	output[0] = '\0';
	filename.lStructSize = sizeof(OPENFILENAMEA);
	filename.hwndOwner = owner;
	filename.lpstrFilter = "NES Files\0*.nes\0All Files\0*.*\0";
	filename.lpstrFile = output;
	filename.nMaxFile = output_len;
	filename.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
	
	return GetOpenFileNameA(&filename);

}

static void (*XFunctionPtr)(void) = NULL;
void setXFunction(void (*funcPtr)(void)) { XFunctionPtr = funcPtr; }

void handleMenuEvents(SDL_Event e) {
	switch (e.type) {
		// X button is pressed
		case SDL_QUIT:
			if (XFunctionPtr != NULL) { (*XFunctionPtr)(); }
			break;
		case SDL_SYSWMEVENT:
			// An item was clicked on in the menuu
			if (e.syswm.msg->msg.win.msg == WM_COMMAND) {
				// Switch based on the ID of the item clicked on
				switch (LOWORD(e.syswm.msg->msg.win.wParam)) {
					case MENU_ID_LOAD_ROM: {
						char buffer[MAX_ROM_PATH_LENGTH];
						loadROMDialog(getHWND(), buffer, MAX_ROM_PATH_LENGTH);
						if (buffer[0] == '\0') { break; }
						uint8_t* rom_data = NES_readROMtoBuffer(buffer);
						if (rom_data == NULL) {
							printf("Error: Failed to load ROM.\n");
							break;
						}
						NES_resetConsoleState(pairedConsole);
						struct Cartridge* game_cart = NES_createCartridgeFromBuffer(rom_data);
						free(rom_data);
						if (NES_insertCartridge(pairedConsole, game_cart) == 1) {
							printf("Error: Failed to insert cartridge.\n");
							break;
						}
						printf("ROM loaded: %s\n", buffer);
						break;
					}
					case MENU_ID_FILE_EXIT:
						if (XFunctionPtr != NULL) { (*XFunctionPtr)(); }
						break;
					default:
						break;
				}
			}
			break;
		default:
			break;
	}
}

void pairConsoleToWindow(struct NES_Console *console) {
	pairedConsole = console;
}