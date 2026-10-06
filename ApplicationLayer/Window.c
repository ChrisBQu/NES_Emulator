#include "Window.h"
#include "Application.h"
#include "Controller.h"
#include "Screen.h"
#include "Overlay.h"
#include "Screenshot.h"
#include "../EmulationLayer/Bus.h"
#include "../EmulationLayer/Cartridge.h"
#include "../EmulationLayer/Savestate.h"
#include "Dialogs/ConfigControllers.h"
#include "Dialogs/ConfigCrt.h"
#include "Dialogs/ConfigSound.h"
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
static HMENU scaleMenu = NULL;
static HMENU configMenu = NULL;

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
	MENU_ID_SCALE_1X,
	MENU_ID_SCALE_2X,
	MENU_ID_SCALE_3X,
	MENU_ID_SCALE_4X,
	MENU_ID_FULLSCREEN,
	MENU_ID_CONFIG_CRT,
	MENU_ID_CONFIG_SOUND,
	MENU_ID_SCREENSHOT,
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
	configMenu = CreatePopupMenu();
	HMENU saveSavestateMenu = CreatePopupMenu();
	HMENU loadSavestateMenu = CreatePopupMenu();
	scaleMenu = CreatePopupMenu();
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

	AppendMenuA(configMenu, MF_STRING, MENU_ID_CONFIG_CONTROLLERS, "Configure Controllers...");
	AppendMenuA(configMenu, MF_SEPARATOR, 0, NULL);
	AppendMenuA(configMenu, MF_POPUP, (UINT_PTR)scaleMenu, "Screen Scale");
	AppendMenuA(scaleMenu, MF_STRING, MENU_ID_SCALE_1X, "1x");
	AppendMenuA(scaleMenu, MF_STRING, MENU_ID_SCALE_2X, "2x");
	AppendMenuA(scaleMenu, MF_STRING, MENU_ID_SCALE_3X, "3x");
	AppendMenuA(scaleMenu, MF_STRING, MENU_ID_SCALE_4X, "4x");
	CheckMenuRadioItem(scaleMenu, MENU_ID_SCALE_1X, MENU_ID_SCALE_4X, MENU_ID_SCALE_1X + DEFAULT_SCREEN_SCALE - 1, MF_BYCOMMAND);
	AppendMenuA(configMenu, MF_STRING, MENU_ID_FULLSCREEN, "Fullscreen\tF11");
	AppendMenuA(configMenu, MF_STRING, MENU_ID_CONFIG_CRT, "Configure CRT Filter...");
	AppendMenuA(configMenu, MF_SEPARATOR, 0, NULL);
	AppendMenuA(configMenu, MF_STRING, MENU_ID_CONFIG_SOUND, "Configure Sound...");
	AppendMenuA(menuBar, MF_POPUP, (UINT_PTR)fileMenu, "File");
	AppendMenuA(menuBar, MF_POPUP, (UINT_PTR)configMenu, "Config");

	HMENU miscMenu = CreatePopupMenu();
	AppendMenuA(miscMenu, MF_STRING, MENU_ID_SCREENSHOT, "Take Screenshot\tF12");
	AppendMenuA(menuBar, MF_POPUP, (UINT_PTR)miscMenu, "Misc");


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
	// The screen draws with OpenGL 3.3 shaders. These have to be set before the window is created
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

	// Attempt to create the window, and bail if it doesn't work
	window = SDL_CreateWindow(screen_name, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, screen_w, screen_h, SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL);
	if (window == NULL) {
		printf("Error: Window could not be created.");
		return false;
	}

	initMenus();

	return true;
}

static bool isFullscreen = false;
static int windowedWidth = 0;
static int windowedHeight = 0;

static void setFullscreen(bool fullscreen) {
	if (fullscreen == isFullscreen) { return; }
	isFullscreen = fullscreen;
	HWND hwnd = getHWND();
	if (fullscreen) {
		// Remember the windowed size so it can be restored. The menu bar would take space from the top of the screen, so hide it
		SDL_GetWindowSize(window, &windowedWidth, &windowedHeight);
		if (hwnd != NULL) { SetMenu(hwnd, NULL); }
		SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
		SDL_ShowCursor(SDL_DISABLE);
	}
	else {
		SDL_SetWindowFullscreen(window, 0);
		if (hwnd != NULL) { SetMenu(hwnd, menuBar); }
		// Re-adding the menu shrinks the drawable area, so set the size again to get it back
		SDL_SetWindowSize(window, windowedWidth, windowedHeight);
		SDL_ShowCursor(SDL_ENABLE);
	}
}

// Leaves fullscreen first, so the new window size is applied to the window rather than lost
static void setWindowScale(int scale) {
	setFullscreen(false);
	SDL_SetWindowSize(window, DEFAULT_SCREEN_WIDTH * scale, DEFAULT_SCREEN_HEIGHT * scale);
	CheckMenuRadioItem(scaleMenu, MENU_ID_SCALE_1X, MENU_ID_SCALE_4X, MENU_ID_SCALE_1X + scale - 1, MF_BYCOMMAND);
}

SDL_Window* getWindow() {
	return window;
}

void setCrtFilterEnabled(bool enabled) {
	struct ApplicationState* appState = getGlobalApplicationState();
	if (appState == NULL) { return; }
	appState->options.crtFilterEnabled = enabled;
	setScreenShader(appState->screen, enabled ? SCREEN_SHADER_CRT : SCREEN_SHADER_NORMAL);
}

void updateMenusFromApplicationState() {
	struct ApplicationState* appState = getGlobalApplicationState();
	if (appState == NULL) { return; }
	if (appState->options.screenScale < 1 || appState->options.screenScale > 4) { 
		printf("Warning: Invalid screen scale %d, using 1x instead.\n", appState->options.screenScale);
		appState->options.screenScale = 1; 
	} else {
		CheckMenuRadioItem(scaleMenu, MENU_ID_SCALE_1X, MENU_ID_SCALE_4X, MENU_ID_SCALE_1X + appState->options.screenScale - 1, MF_BYCOMMAND);
	}
	setWindowScale(appState->options.screenScale);
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

// Save a screenshot, with the CRT filter if it's on, and show a thumbnail of it (or say it failed) on screen
static void screenshot() {
	int width, height;
	uint8_t* rgb = readScreenPixels(getGlobalApplicationState()->screen, getGlobalApplicationState()->options.crtFilterEnabled, &width, &height);
	char path[MAX_SCREENSHOT_PATH_LENGTH];
	if (rgb != NULL && saveScreenshot(rgb, width, height, path, sizeof(path))) {
		printf("Screenshot saved: %s\n", path);
		overlayShowMessage(getGlobalApplicationState()->overlay, "Screenshot saved");
		overlayShowThumbnail(getGlobalApplicationState()->overlay, rgb, width, height);
	}
	else {
		overlayShowMessage(getGlobalApplicationState()->overlay, "Failed to save screenshot");
	}
	free(rgb);
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
						NES_resetConsoleState(getGlobalApplicationState()->console);
						NES_clearHistory();
						struct Cartridge* game_cart = NES_createCartridgeFromBuffer(rom_data);
						free(rom_data);
						if (NES_insertCartridge(getGlobalApplicationState()->console, game_cart) == 1) {
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
					case MENU_ID_SAVE_SAVESLOT_6: case MENU_ID_SAVE_SAVESLOT_7: case MENU_ID_SAVE_SAVESLOT_8: case MENU_ID_SAVE_SAVESLOT_9: case MENU_ID_SAVE_SAVESLOT_10: {
						int slot = LOWORD(e.syswm.msg->msg.win.wParam) - MENU_ID_SAVE_SAVESLOT_1;
						char message[MAX_OVERLAY_TEXT_LENGTH];
						if (NES_saveSavestateSlot(getGlobalApplicationState()->console, slot) == 0) { SDL_snprintf(message, sizeof(message), "Saved to slot %d", slot + 1); }
						else { SDL_snprintf(message, sizeof(message), "Failed to save to slot %d", slot + 1); }
						overlayShowMessage(getGlobalApplicationState()->overlay, message);
						break;
					}
					case MENU_ID_SAVE_SAVESLOT_FILE: {
						char buffer[MAX_SAVESTATE_FILE_PATH_LENGTH];
						saveSavestateDialog(getHWND(), buffer, MAX_SAVESTATE_FILE_PATH_LENGTH);
						if (buffer[0] == '\0') { break; }
						if (NES_saveSavestateToFile(getGlobalApplicationState()->console, buffer) == 0) { overlayShowMessage(getGlobalApplicationState()->overlay, "Savestate saved"); }
						else { overlayShowMessage(getGlobalApplicationState()->overlay, "Failed to save savestate"); }
						break;
					}
					case MENU_ID_LOAD_SAVESLOT_FILE: {
						char buffer[MAX_SAVESTATE_FILE_PATH_LENGTH];
						loadSavestateDialog(getHWND(), buffer, MAX_SAVESTATE_FILE_PATH_LENGTH);
						if (buffer[0] == '\0') { break; }
						struct NES_Savestate* savestate = NES_loadSavestateFromFile(getGlobalApplicationState()->console, buffer);
						if (savestate == NULL) {
							printf("Error: Failed to load savestate.\n");
							overlayShowMessage(getGlobalApplicationState()->overlay, "Failed to load savestate");
							break;
						}
						// Reading the file only builds the savestate. Apply it to the console, then free it either way
						if (NES_loadSavestate(getGlobalApplicationState()->console, savestate) == 0) {
							overlayShowMessage(getGlobalApplicationState()->overlay, "Savestate loaded");
						}
						else {
							printf("Error: Failed to load savestate.\n");
							overlayShowMessage(getGlobalApplicationState()->overlay, "Failed to load savestate");
						}
						NES_freeSavestate(savestate);
						break;
					}
					case MENU_ID_LOAD_SAVESLOT_1: case MENU_ID_LOAD_SAVESLOT_2: case MENU_ID_LOAD_SAVESLOT_3: case MENU_ID_LOAD_SAVESLOT_4: case MENU_ID_LOAD_SAVESLOT_5:
					case MENU_ID_LOAD_SAVESLOT_6: case MENU_ID_LOAD_SAVESLOT_7: case MENU_ID_LOAD_SAVESLOT_8: case MENU_ID_LOAD_SAVESLOT_9: case MENU_ID_LOAD_SAVESLOT_10: {
						int slot = LOWORD(e.syswm.msg->msg.win.wParam) - MENU_ID_LOAD_SAVESLOT_1;
						char message[MAX_OVERLAY_TEXT_LENGTH];
						if (NES_loadSavestateSlot(getGlobalApplicationState()->console, slot) == 0) { SDL_snprintf(message, sizeof(message), "Loaded slot %d", slot + 1); }
						else { SDL_snprintf(message, sizeof(message), "Failed to load slot %d", slot + 1); }
						overlayShowMessage(getGlobalApplicationState()->overlay, message);
						break;
					}
					case MENU_ID_CONFIG_CONTROLLERS:
						DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(ID_DIALOG_CONFIGURE_CONTROLLERS), getHWND(), (DLGPROC)getConfigControllersDialogProc());
						break;
					case MENU_ID_SCALE_1X: case MENU_ID_SCALE_2X: case MENU_ID_SCALE_3X: case MENU_ID_SCALE_4X: {
						int scale = LOWORD(e.syswm.msg->msg.win.wParam) - MENU_ID_SCALE_1X + 1;
						setWindowScale(scale);
						getGlobalApplicationState()->options.screenScale = scale;
						saveApplicationOptions(getGlobalApplicationState());
						break;
					}
					case MENU_ID_FULLSCREEN:
						setFullscreen(true);
						break;
					case MENU_ID_CONFIG_CRT:
						DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(ID_DIALOG_CONFIGURE_CRT), getHWND(), (DLGPROC)getConfigCrtDialogProc());
						break;
					case MENU_ID_CONFIG_SOUND:
						DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(ID_DIALOG_CONFIGURE_SOUND), getHWND(), (DLGPROC)getConfigSoundDialogProc());
						break;
					case MENU_ID_SCREENSHOT:
						screenshot();
						break;
					default:
						break;
				}
			}
			break;
		case SDL_KEYDOWN:
			// F11 toggles fullscreen. Escape also leaves it, since the menu is hidden while fullscreen
			if (e.key.repeat) { break; }
			if (e.key.keysym.sym == SDLK_F11) { setFullscreen(!isFullscreen); }
			else if (e.key.keysym.sym == SDLK_ESCAPE) { setFullscreen(false); }
			else if (e.key.keysym.sym == SDLK_F12) { screenshot(); }
			break;
		default:
			break;
	}
}