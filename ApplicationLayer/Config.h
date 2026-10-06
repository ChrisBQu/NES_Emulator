#ifndef CONFIG_H
#define CONFIG_H

#include "Controller.h"
#include "Screen.h"

struct Config {
    int crtFilterEnabled;
    struct CrtSettings crt;
    int screenScale;
    int soundEnabled;
    int soundVolume;    // Percent, from 0 to 100
    char* bindings[2][NUMBER_OF_BUTTONS];
};

void setConfigDefaults(struct Config *cfg);
int loadConfigFile(struct Config *cfg, const char* filename);
int saveConfigFile(const struct Config *cfg, const char* filename);
void freeConfig(struct Config *cfg);
#endif