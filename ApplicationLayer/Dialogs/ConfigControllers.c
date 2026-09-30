#include "ConfigControllers.h"
#include "../Controller.h"
#include "../resource.h"

#define MAX_BINDING_TEXT_LENGTH 128

static Controller* pairedControllers[2] = { NULL, NULL };

// The controller dialog's text box for each NES button, in CONTROLLER_BUTTON order (A, B, Select, Start, Up, Down, Left, Right)
static const int bindingTextIDs[2][NUMBER_OF_BUTTONS] = {
	{ IDC_CONTROLLER_P1_A, IDC_CONTROLLER_P1_B, IDC_CONTROLLER_P1_SELECT, IDC_CONTROLLER_P1_START,
	  IDC_CONTROLLER_P1_UP, IDC_CONTROLLER_P1_DOWN, IDC_CONTROLLER_P1_LEFT, IDC_CONTROLLER_P1_RIGHT },
	{ IDC_CONTROLLER_P2_A, IDC_CONTROLLER_P2_B, IDC_CONTROLLER_P2_SELECT, IDC_CONTROLLER_P2_START,
	  IDC_CONTROLLER_P2_UP, IDC_CONTROLLER_P2_DOWN, IDC_CONTROLLER_P2_LEFT, IDC_CONTROLLER_P2_RIGHT }
};

// Helper function: fill every text box in the controller dialog with what's bound to its button
static void showBindings(HWND dlg) {
	char text[MAX_BINDING_TEXT_LENGTH];
	for (int player = 0; player < 2; player++) {
		for (int b = 0; b < NUMBER_OF_BUTTONS; b++) {
			SetDlgItemTextA(dlg, bindingTextIDs[player][b], getBindingAsString(pairedControllers[player], (CONTROLLER_BUTTON)b, text, sizeof(text)));
		}
	}
}

// Messages the binding dialogs send themselves. WM_APP and above are free for programs to use
#define WM_APP_BINDING_BOX_CLICKED (WM_APP + 1)   // wParam: the clicked box's control ID
#define WM_APP_KEY_CAPTURED        (WM_APP + 2)   // wParam: the SDL_Keycode that was pressed
#define CAPTURE_POLL_TIMER_ID 1
#define CAPTURE_POLL_INTERVAL_MS 16

// Helper function: convert a Windows key press into the SDL keycode SDL would report for it, since SDL has no public
// function for this. Returns SDLK_UNKNOWN for keys that aren't handled
static SDL_Keycode vkToSDLKey(WPARAM vk, LPARAM lParam) {
	bool extended = (lParam >> 24) & 1;   // Tells apart the right-hand Ctrl/Alt, and the numpad Enter
	if (vk >= 'A' && vk <= 'Z') { return (SDL_Keycode)('a' + (vk - 'A')); }
	if (vk >= '0' && vk <= '9') { return (SDL_Keycode)vk; }
	if (vk >= VK_F1 && vk <= VK_F12) { return SDLK_F1 + (SDL_Keycode)(vk - VK_F1); }
	if (vk == VK_NUMPAD0) { return SDLK_KP_0; }
	if (vk >= VK_NUMPAD1 && vk <= VK_NUMPAD9) { return SDLK_KP_1 + (SDL_Keycode)(vk - VK_NUMPAD1); }
	switch (vk) {
		case VK_UP: return SDLK_UP;
		case VK_DOWN: return SDLK_DOWN;
		case VK_LEFT: return SDLK_LEFT;
		case VK_RIGHT: return SDLK_RIGHT;
		case VK_RETURN: return extended ? SDLK_KP_ENTER : SDLK_RETURN;
		case VK_SPACE: return SDLK_SPACE;
		case VK_ESCAPE: return SDLK_ESCAPE;
		case VK_TAB: return SDLK_TAB;
		case VK_BACK: return SDLK_BACKSPACE;
		case VK_INSERT: return SDLK_INSERT;
		case VK_DELETE: return SDLK_DELETE;
		case VK_HOME: return SDLK_HOME;
		case VK_END: return SDLK_END;
		case VK_PRIOR: return SDLK_PAGEUP;
		case VK_NEXT: return SDLK_PAGEDOWN;
		case VK_CONTROL: return extended ? SDLK_RCTRL : SDLK_LCTRL;
		case VK_MENU: return extended ? SDLK_RALT : SDLK_LALT;
		case VK_SHIFT: {
			// Windows doesn't say which Shift, but the key's scan code does
			UINT scancode = (lParam >> 16) & 0xFF;
			return (MapVirtualKeyA(scancode, MAPVK_VSC_TO_VK_EX) == VK_RSHIFT) ? SDLK_RSHIFT : SDLK_LSHIFT;
		}
		case VK_MULTIPLY: return SDLK_KP_MULTIPLY;
		case VK_ADD: return SDLK_KP_PLUS;
		case VK_SUBTRACT: return SDLK_KP_MINUS;
		case VK_DECIMAL: return SDLK_KP_PERIOD;
		case VK_DIVIDE: return SDLK_KP_DIVIDE;
		// Punctuation, for a US keyboard layout
		case VK_OEM_1: return SDLK_SEMICOLON;
		case VK_OEM_PLUS: return SDLK_EQUALS;
		case VK_OEM_COMMA: return SDLK_COMMA;
		case VK_OEM_MINUS: return SDLK_MINUS;
		case VK_OEM_PERIOD: return SDLK_PERIOD;
		case VK_OEM_2: return SDLK_SLASH;
		case VK_OEM_3: return SDLK_BACKQUOTE;
		case VK_OEM_4: return SDLK_LEFTBRACKET;
		case VK_OEM_5: return SDLK_BACKSLASH;
		case VK_OEM_6: return SDLK_RIGHTBRACKET;
		case VK_OEM_7: return SDLK_QUOTE;
		default: return SDLK_UNKNOWN;
	}
}

// The NES button the capture dialog is binding, passed to it when it opens
struct BindingRequest {
	Controller* controller;
	CONTROLLER_BUTTON button;
};

// The capture dialog's text box catches key presses. Normally a dialog uses Enter, Esc, Tab and the arrows itself,
// so the box asks for every key, and sends the first one it can convert to the dialog
static WNDPROC originalCaptureBoxProc = NULL;
static LRESULT CALLBACK captureBoxProc(HWND box, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg) {
		case WM_GETDLGCODE:
			return DLGC_WANTALLKEYS;
		case WM_KEYDOWN:
		case WM_SYSKEYDOWN: { // WM_SYSKEYDOWN is Alt, and keys pressed while Alt is held
			SDL_Keycode key = vkToSDLKey(wParam, lParam);
			if (key != SDLK_UNKNOWN) { PostMessage(GetParent(box), WM_APP_KEY_CAPTURED, (WPARAM)key, 0); }
			return 0;
		}
		case WM_CHAR:
		case WM_SYSCHAR:  // Swallowed, so keys don't type into the box or beep
			return 0;
		case WM_SETFOCUS: {
			LRESULT result = CallWindowProc(originalCaptureBoxProc, box, msg, wParam, lParam);
			HideCaret(box);
			return result;
		}
	}
	return CallWindowProc(originalCaptureBoxProc, box, msg, wParam, lParam);
}

// A small dialog that shows an NES button's current binding, and stays open until a key, gamepad button or stick
// movement is pressed. That input becomes the new binding
static INT_PTR CALLBACK captureBindingDialogProc(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam) {
	struct BindingRequest* request = (struct BindingRequest*)GetWindowLongPtr(dlg, DWLP_USER);
	switch (msg) {
		case WM_INITDIALOG: {
			request = (struct BindingRequest*)lParam;
			SetWindowLongPtr(dlg, DWLP_USER, (LONG_PTR)request);

			char current[MAX_BINDING_TEXT_LENGTH];
			char prompt[MAX_BINDING_TEXT_LENGTH * 2];
			snprintf(prompt, sizeof(prompt), "Press a key or button.");
			HWND box = GetDlgItem(dlg, IDC_CAPTURE_BINDING_TEXT);
			SetWindowTextA(box, prompt);
			originalCaptureBoxProc = (WNDPROC)SetWindowLongPtr(box, GWLP_WNDPROC, (LONG_PTR)captureBoxProc);
			SetFocus(box);

			// SDL's event loop is paused while a dialog is open, so check the gamepad directly on a timer
			SetTimer(dlg, CAPTURE_POLL_TIMER_ID, CAPTURE_POLL_INTERVAL_MS, NULL);
			return FALSE; // We set the focus ourselves
		}
		case WM_TIMER:
			SDL_GameControllerUpdate();
			if (controller_bind_from_pad_state(request->controller, request->button)) { EndDialog(dlg, IDOK); }
			return TRUE;
		case WM_APP_KEY_CAPTURED:
			controller_bind_key_to_button(request->controller, (SDL_Keycode)wParam, request->button);
			EndDialog(dlg, IDOK);
			return TRUE;
		case WM_DESTROY:
			KillTimer(dlg, CAPTURE_POLL_TIMER_ID);
			return TRUE;
	}
	return FALSE;
}

// The controller dialog's binding boxes are read-only text boxes, which don't report clicks. This replacement message
// handler passes a click on to the dialog, then lets the box behave normally
static WNDPROC originalBindingBoxProc = NULL;
static LRESULT CALLBACK bindingBoxProc(HWND box, UINT msg, WPARAM wParam, LPARAM lParam) {
	// Posted rather than handled here, so the capture dialog opens after the click has finished
	if (msg == WM_LBUTTONUP) { PostMessage(GetParent(box), WM_APP_BINDING_BOX_CLICKED, (WPARAM)GetDlgCtrlID(box), 0); }
	return CallWindowProc(originalBindingBoxProc, box, msg, wParam, lParam);
}

static INT_PTR CALLBACK controlsDialogProc(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg) {
	case WM_INITDIALOG:
		showBindings(dlg);
        for (uint8_t i = 0; i < NUMBER_OF_BUTTONS; i++) {
            originalBindingBoxProc = (WNDPROC)SetWindowLongPtr(GetDlgItem(dlg, bindingTextIDs[0][i]), GWLP_WNDPROC, (LONG_PTR)bindingBoxProc);
        }
        for (uint8_t i = 0; i < NUMBER_OF_BUTTONS; i++) {
            originalBindingBoxProc = (WNDPROC)SetWindowLongPtr(GetDlgItem(dlg, bindingTextIDs[1][i]), GWLP_WNDPROC, (LONG_PTR)bindingBoxProc);
        }
		return TRUE;
	case WM_APP_BINDING_BOX_CLICKED:
		for (uint8_t i = 0; i < NUMBER_OF_BUTTONS; i++) {
			if (wParam == bindingTextIDs[0][i]) {
				struct BindingRequest request = { pairedControllers[0], (CONTROLLER_BUTTON)i };
				DialogBoxParam(GetModuleHandle(NULL), MAKEINTRESOURCE(ID_DIALOG_CAPTURE_BINDING), dlg, captureBindingDialogProc, (LPARAM)&request);
				showBindings(dlg);
				break;
			}
		}
        // Set on-click handlers for all controller 2 buttons
		for (uint8_t i = 0; i < NUMBER_OF_BUTTONS; i++) {
			if (wParam == bindingTextIDs[1][i]) {
				struct BindingRequest request = { pairedControllers[1], (CONTROLLER_BUTTON)i };
				DialogBoxParam(GetModuleHandle(NULL), MAKEINTRESOURCE(ID_DIALOG_CAPTURE_BINDING), dlg, captureBindingDialogProc, (LPARAM)&request);
				showBindings(dlg);
				break;
			}
		}
		return TRUE;
	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK) {
			EndDialog(dlg, LOWORD(wParam));
			return TRUE;
		}
		break;
	case WM_CLOSE:
		EndDialog(dlg, IDCANCEL);
		return TRUE;
	}
	return FALSE;
}

// Exposed function to get the dialog procedure
DLGPROC getConfigControllersDialogProc() {
	return controlsDialogProc;
}

// Receive the controllers to pair to the dialog
void pairControllersToConfigControllersDialog(Controller* player1, Controller* player2) {
	pairedControllers[0] = player1;
	pairedControllers[1] = player2;
}