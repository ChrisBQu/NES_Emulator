#include "ConfigSound.h"
#include <commctrl.h>
#include <stdio.h>

#include "../Application.h"
#include "../resource.h"

// The volume slider is a trackbar, from the common controls library
#pragma comment(lib, "comctl32.lib")

#define MAX_VALUE_TEXT_LENGTH 16

// The options as they were when the dialog opened, put back if it's cancelled
static int originalSoundEnabled;
static int originalSoundVolume;

// Helper function: move every control in the dialog to match the options. The slider can only be used while sound is on
static void showSettings(HWND dlg) {
	struct Config* options = &getGlobalApplicationState()->options;
	CheckDlgButton(dlg, IDC_SOUND_ENABLED, options->soundEnabled ? BST_CHECKED : BST_UNCHECKED);
	SendDlgItemMessage(dlg, IDC_SOUND_VOLUME, TBM_SETPOS, TRUE, options->soundVolume);

	char text[MAX_VALUE_TEXT_LENGTH];
	snprintf(text, sizeof(text), "%d%%", options->soundVolume);
	SetDlgItemTextA(dlg, IDC_SOUND_VOLUME_VALUE, text);

	EnableWindow(GetDlgItem(dlg, IDC_SOUND_VOLUME), options->soundEnabled);
	EnableWindow(GetDlgItem(dlg, IDC_SOUND_VOLUME_VALUE), options->soundEnabled);
}

static INT_PTR CALLBACK soundDialogProc(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam) {
	struct ApplicationState* appState = getGlobalApplicationState();
	switch (msg) {
	case WM_INITDIALOG:
		originalSoundEnabled = appState->options.soundEnabled;
		originalSoundVolume = appState->options.soundVolume;
		SendDlgItemMessage(dlg, IDC_SOUND_VOLUME, TBM_SETRANGE, FALSE, MAKELPARAM(0, 100));
		SendDlgItemMessage(dlg, IDC_SOUND_VOLUME, TBM_SETPAGESIZE, 0, 10);
		showSettings(dlg);
		return TRUE;
	case WM_HSCROLL:
		// Sent while the slider is being moved
		if (GetDlgCtrlID((HWND)lParam) == IDC_SOUND_VOLUME) {
			appState->options.soundVolume = (int)SendDlgItemMessage(dlg, IDC_SOUND_VOLUME, TBM_GETPOS, 0, 0);
			showSettings(dlg);
		}
		return TRUE;
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDC_SOUND_ENABLED:
			appState->options.soundEnabled = IsDlgButtonChecked(dlg, IDC_SOUND_ENABLED) == BST_CHECKED;
			showSettings(dlg);
			return TRUE;
		case IDOK:
			saveApplicationOptions(appState);
			EndDialog(dlg, IDOK);
			return TRUE;
		case IDCANCEL:
			// Also sent by Esc and the X button
			appState->options.soundEnabled = originalSoundEnabled;
			appState->options.soundVolume = originalSoundVolume;
			EndDialog(dlg, IDCANCEL);
			return TRUE;
		}
		break;
	}
	return FALSE;
}

// Exposed function to get the dialog procedure
DLGPROC getConfigSoundDialogProc() {
	// The trackbar class has to be registered before the dialog's slider is created
	INITCOMMONCONTROLSEX commonControls = { sizeof(commonControls), ICC_BAR_CLASSES };
	InitCommonControlsEx(&commonControls);
	return soundDialogProc;
}