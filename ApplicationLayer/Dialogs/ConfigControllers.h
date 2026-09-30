#ifndef CONFIG_CONTROLLERS_H
#define CONFIG_CONTROLLERS_H

#include <windows.h>
#include "../Controller.h"

DLGPROC getConfigControllersDialogProc();
void pairControllersToConfigControllersDialog(Controller* player1, Controller* player2);

#endif