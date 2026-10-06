#include "ConfigCrt.h"
#include <commctrl.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>

#include "../Application.h"
#include "../Config.h"
#include "../Window.h"
#include "../resource.h"

// The sliders are trackbars, from the common controls library
#pragma comment(lib, "comctl32.lib")

#define MAX_VALUE_TEXT_LENGTH 16

// A slider for one of the CRT settings. Sliders only move in whole steps, so the slider's position, from 0 to steps,
// is spread evenly between the setting's value at the left end and its value at the right end
struct CrtSlider {
	int sliderID;
	int valueTextID;        // The text next to the slider that shows the setting's value
	size_t offset;          // Where the setting is in struct CrtSettings
	float leftValue;
	float rightValue;
	int steps;
	const char* format;     // How the value is shown
};

// The hardness settings get harder as they go more negative, so their sliders go from soft on the left to hard on the right
static const struct CrtSlider sliders[] = {
	{ IDC_CRT_HARD_SCAN,  IDC_CRT_HARD_SCAN_VALUE,  offsetof(struct CrtSettings, hardScan),  -2.0f, -24.0f,  44, "%.1f" },
	{ IDC_CRT_HARD_PIX,   IDC_CRT_HARD_PIX_VALUE,   offsetof(struct CrtSettings, hardPix),   -1.0f,  -8.0f,  28, "%.2f" },
	{ IDC_CRT_WARP_H,     IDC_CRT_WARP_H_VALUE,     offsetof(struct CrtSettings, warpH),      0.0f, 0.125f, 100, "%.4f" },
	{ IDC_CRT_WARP_V,     IDC_CRT_WARP_V_VALUE,     offsetof(struct CrtSettings, warpV),      0.0f, 0.125f, 100, "%.4f" },
	{ IDC_CRT_MASK_DARK,  IDC_CRT_MASK_DARK_VALUE,  offsetof(struct CrtSettings, maskDark),   0.0f,   1.0f, 100, "%.2f" },
	{ IDC_CRT_MASK_LIGHT, IDC_CRT_MASK_LIGHT_VALUE, offsetof(struct CrtSettings, maskLight),  1.0f,   2.0f, 100, "%.2f" },
};
#define NUMBER_OF_SLIDERS (sizeof(sliders) / sizeof(sliders[0]))

// The options as they were when the dialog opened, put back if it's cancelled
static int originalCrtFilterEnabled;
static struct CrtSettings originalCrtSettings;

// Helper function: get the setting a slider controls
static float* getSliderSetting(struct CrtSettings* settings, const struct CrtSlider* slider) {
	return (float*)((char*)settings + slider->offset);
}

// Helper function: show a setting's value in the text next to its slider
static void showSliderValue(HWND dlg, const struct CrtSlider* slider, float value) {
	char text[MAX_VALUE_TEXT_LENGTH];
	snprintf(text, sizeof(text), slider->format, value);
	SetDlgItemTextA(dlg, slider->valueTextID, text);
}

// Helper function: move every control in the dialog to match the options. The sliders can only be used while the filter is on
static void showSettings(HWND dlg) {
	struct Config* options = &getGlobalApplicationState()->options;
	CheckDlgButton(dlg, IDC_CRT_ENABLED, options->crtFilterEnabled ? BST_CHECKED : BST_UNCHECKED);
	for (size_t i = 0; i < NUMBER_OF_SLIDERS; i++) {
		const struct CrtSlider* slider = &sliders[i];
		float value = *getSliderSetting(&options->crt, slider);
		long position = lroundf((value - slider->leftValue) / (slider->rightValue - slider->leftValue) * slider->steps);
		SendDlgItemMessage(dlg, slider->sliderID, TBM_SETPOS, TRUE, position);
		showSliderValue(dlg, slider, value);
		EnableWindow(GetDlgItem(dlg, slider->sliderID), options->crtFilterEnabled);
		EnableWindow(GetDlgItem(dlg, slider->valueTextID), options->crtFilterEnabled);
	}
}

// Helper function: give the screen the options' CRT settings, and redraw it so the change shows while the dialog is open
static void applySettings() {
	struct ApplicationState* appState = getGlobalApplicationState();
	setCrtSettings(appState->screen, &appState->options.crt);
	presentAll(appState);
}

// Helper function: set a slider's setting from where the slider has been moved to
static void readSlider(HWND dlg, const struct CrtSlider* slider) {
	struct Config* options = &getGlobalApplicationState()->options;
	LRESULT position = SendDlgItemMessage(dlg, slider->sliderID, TBM_GETPOS, 0, 0);
	float value = slider->leftValue + (slider->rightValue - slider->leftValue) * (float)position / slider->steps;
	*getSliderSetting(&options->crt, slider) = value;
	showSliderValue(dlg, slider, value);
	applySettings();
}

static INT_PTR CALLBACK crtDialogProc(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam) {
	struct ApplicationState* appState = getGlobalApplicationState();
	switch (msg) {
	case WM_INITDIALOG:
		originalCrtFilterEnabled = appState->options.crtFilterEnabled;
		originalCrtSettings = appState->options.crt;
		for (size_t i = 0; i < NUMBER_OF_SLIDERS; i++) {
			SendDlgItemMessage(dlg, sliders[i].sliderID, TBM_SETRANGE, FALSE, MAKELPARAM(0, sliders[i].steps));
		}
		showSettings(dlg);
		return TRUE;
	case WM_HSCROLL:
		// Sent while a slider is being moved. lParam is the slider
		for (size_t i = 0; i < NUMBER_OF_SLIDERS; i++) {
			if (GetDlgCtrlID((HWND)lParam) == sliders[i].sliderID) {
				readSlider(dlg, &sliders[i]);
				break;
			}
		}
		return TRUE;
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDC_CRT_ENABLED:
			setCrtFilterEnabled(IsDlgButtonChecked(dlg, IDC_CRT_ENABLED) == BST_CHECKED);
			showSettings(dlg);
			applySettings();
			return TRUE;
		case IDC_CRT_DEFAULTS: {
			// Only the CRT settings go back to their defaults. Whether the filter is on stays as it is
			struct Config defaults = { 0 };
			setConfigDefaults(&defaults);
			appState->options.crt = defaults.crt;
			freeConfig(&defaults);
			showSettings(dlg);
			applySettings();
			return TRUE;
		}
		case IDOK:
			saveApplicationOptions(appState);
			EndDialog(dlg, IDOK);
			return TRUE;
		case IDCANCEL:
			// Also sent by Esc and the X button
			appState->options.crt = originalCrtSettings;
			setCrtFilterEnabled(originalCrtFilterEnabled);
			applySettings();
			EndDialog(dlg, IDCANCEL);
			return TRUE;
		}
		break;
	}
	return FALSE;
}

// Exposed function to get the dialog procedure
DLGPROC getConfigCrtDialogProc() {
	// The trackbar class has to be registered before the dialog's sliders are created
	INITCOMMONCONTROLSEX commonControls = { sizeof(commonControls), ICC_BAR_CLASSES };
	InitCommonControlsEx(&commonControls);
	return crtDialogProc;
}