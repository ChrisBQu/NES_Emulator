#include "Window.h"
#include "Controller.h"
#include "../EmulationLayer/Bus.h"
#include "../EmulationLayer/Cartridge.h"
#include "../EmulationLayer/Savestate.h"
#include "Dialogs/ConfigControllers.h"
#include "resource.h"
#include <stdio.h>
#include <SDL.h>
#include <SDL_syswm.h>
#include <windows.h>
#include <commdlg.h>

#define MAX_ROM_PATH_LENGTH 1024
#define MAX_SAVESTATE_FILE_PATH_LENGTH 1024

static SDL_Window* window = NULL;
static HMENU menuBar = NULL;
static struct NES_Console* pairedConsole = NULL;
static Controller* pairedControllers[2] = { NULL, NULL };

enum MENU_IDS {
	MENU_ID_LOAD_ROM = 1001,
	MENU_ID_SAVE_SAVESLOT_1,
	MENU_ID_SAVE_SAVESLOT_2,
	MENU_ID_SAVE_SAVESLOT_3,
	MENU_ID_SAVE_SAVESLOT_4,
	MENU_ID_SAVE_SAVESLOT_5,
	MENU_ID_SAVE_SAVESLOT_6,
	MENU_ID_SAVE_SAVESLOT_7,
	MENU_ID_SAVE_SAVESLOT_8,
	MENU_ID_SAVE_SAVESLOT_9,
	MENU_ID_SAVE_SAVESLOT_10,
	MENU_ID_SAVE_SAVESLOT_FILE,
	MENU_ID_LOAD_SAVESLOT_1,
	MENU_ID_LOAD_SAVESLOT_2,
	MENU_ID_LOAD_SAVESLOT_3,
	MENU_ID_LOAD_SAVESLOT_4,
	MENU_ID_LOAD_SAVESLOT_5,
	MENU_ID_LOAD_SAVESLOT_6,
	MENU_ID_LOAD_SAVESLOT_7,
	MENU_ID_LOAD_SAVESLOT_8,
	MENU_ID_LOAD_SAVESLOT_9,
	MENU_ID_LOAD_SAVESLOT_10,	
	MENU_ID_LOAD_SAVESLOT_FILE,
	MENU_ID_FILE_EXIT,
	MENU_ID_CONFIG_CONTROLLERS,
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
	HMENU configMenu = CreatePopupMenu();
	HMENU saveSavestateMenu = CreatePopupMenu();
	HMENU loadSavestateMenu = CreatePopupMenu();
	AppendMenuA(fileMenu, MF_STRING, MENU_ID_LOAD_ROM, "Load ROM...");
	AppendMenuA(fileMenu, MF_SEPARATOR, 0, NULL);
	AppendMenuA(fileMenu, MF_POPUP, (UINT_PTR)loadSavestateMenu, "Load Savestate...");
	AppendMenuA(loadSavestateMenu, MF_STRING, MENU_ID_LOAD_SAVESLOT_1, "Load Savestate Slot 1");
	AppendMenuA(loadSavestateMenu, MF_STRING, MENU_ID_LOAD_SAVESLOT_2, "Load Savestate Slot 2");
	AppendMenuA(loadSavestateMenu, MF_STRING, MENU_ID_LOAD_SAVESLOT_3, "Load Savestate Slot 3");
	AppendMenuA(loadSavestateMenu, MF_STRING, MENU_ID_LOAD_SAVESLOT_4, "Load Savestate Slot 4");
	AppendMenuA(loadSavestateMenu, MF_STRING, MENU_ID_LOAD_SAVESLOT_5, "Load Savestate Slot 5");
	AppendMenuA(loadSavestateMenu, MF_STRING, MENU_ID_LOAD_SAVESLOT_6, "Load Savestate Slot 6");
	AppendMenuA(loadSavestateMenu, MF_STRING, MENU_ID_LOAD_SAVESLOT_7, "Load Savestate Slot 7");
	AppendMenuA(loadSavestateMenu, MF_STRING, MENU_ID_LOAD_SAVESLOT_8, "Load Savestate Slot 8");
	AppendMenuA(loadSavestateMenu, MF_STRING, MENU_ID_LOAD_SAVESLOT_9, "Load Savestate Slot 9");
	AppendMenuA(loadSavestateMenu, MF_STRING, MENU_ID_LOAD_SAVESLOT_10, "Load Savestate Slot 10");
	AppendMenuA(loadSavestateMenu, MF_STRING, MENU_ID_LOAD_SAVESLOT_FILE, "Load Savestate from File...");
	AppendMenuA(fileMenu, MF_POPUP, (UINT_PTR)saveSavestateMenu, "Save Savestate...");
	AppendMenuA(saveSavestateMenu, MF_STRING, MENU_ID_SAVE_SAVESLOT_1, "Save Savestate Slot 1");
	AppendMenuA(saveSavestateMenu, MF_STRING, MENU_ID_SAVE_SAVESLOT_2, "Save Savestate Slot 2");
	AppendMenuA(saveSavestateMenu, MF_STRING, MENU_ID_SAVE_SAVESLOT_3, "Save Savestate Slot 3");
	AppendMenuA(saveSavestateMenu, MF_STRING, MENU_ID_SAVE_SAVESLOT_4, "Save Savestate Slot 4");
	AppendMenuA(saveSavestateMenu, MF_STRING, MENU_ID_SAVE_SAVESLOT_5, "Save Savestate Slot 5");
	AppendMenuA(saveSavestateMenu, MF_STRING, MENU_ID_SAVE_SAVESLOT_6, "Save Savestate Slot 6");
	AppendMenuA(saveSavestateMenu, MF_STRING, MENU_ID_SAVE_SAVESLOT_7, "Save Savestate Slot 7");
	AppendMenuA(saveSavestateMenu, MF_STRING, MENU_ID_SAVE_SAVESLOT_8, "Save Savestate Slot 8");
	AppendMenuA(saveSavestateMenu, MF_STRING, MENU_ID_SAVE_SAVESLOT_9, "Save Savestate Slot 9");
	AppendMenuA(saveSavestateMenu, MF_STRING, MENU_ID_SAVE_SAVESLOT_10, "Save Savestate Slot 10");
	AppendMenuA(saveSavestateMenu, MF_STRING, MENU_ID_SAVE_SAVESLOT_FILE, "Save Savestate to File...");
	AppendMenuA(fileMenu, MF_SEPARATOR, 0, NULL);
	AppendMenuA(fileMenu, MF_STRING, MENU_ID_FILE_EXIT, "Exit");

	AppendMenuA(configMenu, MF_STRING, MENU_ID_CONFIG_CONTROLLERS, "Configure Controllers");

	AppendMenuA(menuBar, MF_POPUP, (UINT_PTR)fileMenu, "File");
	AppendMenuA(menuBar, MF_POPUP, (UINT_PTR)configMenu, "Config");


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

static int saveSavestateDialog(HWND owner, char *output, DWORD output_len) {
	OPENFILENAMEA filename = {0};
	output[0] = '\0';
	filename.lStructSize = sizeof(OPENFILENAMEA);
	filename.hwndOwner = owner;
	filename.lpstrFilter = "Savestate Files\0*.savestate\0All Files\0*.*\0";
	filename.lpstrFile = output;
	filename.nMaxFile = output_len;
	filename.lpstrDefExt = "savestate";
	filename.Flags = OFN_EXPLORER | OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
	return GetSaveFileNameA(&filename);
}

static int loadSavestateDialog(HWND owner, char *output, DWORD output_len) {
	OPENFILENAMEA filename = {0};
	output[0] = '\0';
	filename.lStructSize = sizeof(OPENFILENAMEA);
	filename.hwndOwner = owner;
	filename.lpstrFilter = "Savestate Files\0*.savestate\0All Files\0*.*\0";
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
					case MENU_ID_SAVE_SAVESLOT_1: case MENU_ID_SAVE_SAVESLOT_2: case MENU_ID_SAVE_SAVESLOT_3: case MENU_ID_SAVE_SAVESLOT_4: case MENU_ID_SAVE_SAVESLOT_5:
					case MENU_ID_SAVE_SAVESLOT_6: case MENU_ID_SAVE_SAVESLOT_7: case MENU_ID_SAVE_SAVESLOT_8: case MENU_ID_SAVE_SAVESLOT_9: case MENU_ID_SAVE_SAVESLOT_10:
						NES_saveSavestateSlot(pairedConsole, LOWORD(e.syswm.msg->msg.win.wParam) - MENU_ID_SAVE_SAVESLOT_1);
						break;
					case MENU_ID_SAVE_SAVESLOT_FILE: {
						char buffer[MAX_SAVESTATE_FILE_PATH_LENGTH];
						saveSavestateDialog(getHWND(), buffer, MAX_SAVESTATE_FILE_PATH_LENGTH);
						if (buffer[0] == '\0') { break; }
						NES_saveSavestateToFile(pairedConsole, buffer);
						break;
					}
					case MENU_ID_LOAD_SAVESLOT_FILE: {
						char buffer[MAX_SAVESTATE_FILE_PATH_LENGTH];
						loadSavestateDialog(getHWND(), buffer, MAX_SAVESTATE_FILE_PATH_LENGTH);
						if (buffer[0] == '\0') { break; }
						struct NES_Savestate* savestate = NES_loadSavestateFromFile(pairedConsole, buffer);
						if (savestate == NULL) {
							printf("Error: Failed to load savestate.\n");
							break;
						}
						// Reading the file only builds the savestate. Apply it to the console, then free it either way
						if (NES_loadSavestate(pairedConsole, savestate) != 0) {
							printf("Error: Failed to load savestate.\n");
						}
						NES_freeSavestate(savestate);
						break;
					}
					case MENU_ID_LOAD_SAVESLOT_1: case MENU_ID_LOAD_SAVESLOT_2: case MENU_ID_LOAD_SAVESLOT_3: case MENU_ID_LOAD_SAVESLOT_4: case MENU_ID_LOAD_SAVESLOT_5:
					case MENU_ID_LOAD_SAVESLOT_6: case MENU_ID_LOAD_SAVESLOT_7: case MENU_ID_LOAD_SAVESLOT_8: case MENU_ID_LOAD_SAVESLOT_9: case MENU_ID_LOAD_SAVESLOT_10:
						NES_loadSavestateSlot(pairedConsole, LOWORD(e.syswm.msg->msg.win.wParam) - MENU_ID_LOAD_SAVESLOT_1);
						break;
					case MENU_ID_CONFIG_CONTROLLERS:
						DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(ID_DIALOG_CONFIGURE_CONTROLLERS), getHWND(), (DLGPROC)getConfigControllersDialogProc());
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

void pairControllersToWindow(struct Controller* player1, struct Controller* player2) {
	pairedControllers[0] = player1;
	pairedControllers[1] = player2;
	pairControllersToConfigControllersDialog(player1, player2);
}