#define _CRT_SECURE_NO_WARNINGS
#include "Config.h"
#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum ConfigOptionType { CFG_INT, CFG_FLOAT, CFG_STRING };

struct ConfigOption {
    const char* key;
    enum ConfigOptionType type;
    size_t offset;
    const char* defaultValue;   // Written as it would be in the config file
};

// A binding option: the name of what's bound to one of a player's buttons
#define BINDING_OPTION(key, player, button, defaultValue) { key, CFG_STRING, offsetof(struct Config, bindings[player][button]), defaultValue }

// Thesse are the default values that will be set if a config file is not present
static const struct ConfigOption options[] = {
    { "crtFilterEnabled", CFG_INT, offsetof(struct Config, crtFilterEnabled), "1" },
    { "crtHardScan", CFG_FLOAT, offsetof(struct Config, crt.hardScan), "-8" },
    { "crtHardPix", CFG_FLOAT, offsetof(struct Config, crt.hardPix), "-3" },
    { "crtWarpH", CFG_FLOAT, offsetof(struct Config, crt.warpH), "0.03125" },
    { "crtWarpV", CFG_FLOAT, offsetof(struct Config, crt.warpV), "0.0416667" },
    { "crtMaskDark", CFG_FLOAT, offsetof(struct Config, crt.maskDark), "0.5" },
    { "crtMaskLight", CFG_FLOAT, offsetof(struct Config, crt.maskLight), "1.5" },
    { "screenScale", CFG_INT, offsetof(struct Config, screenScale), "3" },
    { "soundEnabled", CFG_INT, offsetof(struct Config, soundEnabled), "1" },
    { "soundVolume", CFG_INT, offsetof(struct Config, soundVolume), "100" },
    BINDING_OPTION("p1_A", 0, BUTTON_A, "F"),
    BINDING_OPTION("p1_B", 0, BUTTON_B, "D"),
    BINDING_OPTION("p1_Select", 0, BUTTON_SELECT, "Left Shift"),
    BINDING_OPTION("p1_Start", 0, BUTTON_START, "Return"),
    BINDING_OPTION("p1_Up", 0, BUTTON_UP, "Up"),
    BINDING_OPTION("p1_Down", 0, BUTTON_DOWN, "Down"),
    BINDING_OPTION("p1_Left", 0, BUTTON_LEFT, "Left"),
    BINDING_OPTION("p1_Right", 0, BUTTON_RIGHT, "Right"),
    BINDING_OPTION("p2_A", 1, BUTTON_A, "<Unmapped>"),
    BINDING_OPTION("p2_B", 1, BUTTON_B, "<Unmapped>"),
    BINDING_OPTION("p2_Select", 1, BUTTON_SELECT, "<Unmapped>"),
    BINDING_OPTION("p2_Start", 1, BUTTON_START, "<Unmapped>"),
    BINDING_OPTION("p2_Up", 1, BUTTON_UP, "<Unmapped>"),
    BINDING_OPTION("p2_Down", 1, BUTTON_DOWN, "<Unmapped>"),
    BINDING_OPTION("p2_Left", 1, BUTTON_LEFT, "<Unmapped>"),
    BINDING_OPTION("p2_Right", 1, BUTTON_RIGHT, "<Unmapped>"),
    BINDING_OPTION("hotkey_Rewind", 0, BUTTON_REWIND, "Backspace"),
    BINDING_OPTION("hotkey_Turbo", 0, BUTTON_TURBO, "Tab"),
};

#define MAX_LINE_LENGTH 1024
#define NUMBER_OF_OPTIONS (sizeof(options) / sizeof(options[0]))

// Helper function: remove whitespace (including the newline) from both ends of a string, in place
static char* trim(char* s) {
    while (isspace((unsigned char)*s)) { s++; }
    char* end = s + strlen(s);
    while (end > s && isspace((unsigned char)end[-1])) { end--; }
    *end = '\0';
    return s;
}

// Helper function to set the value of a key in the config object
static int setConfigValue(struct Config *cfg, const char* key, const char* val) {
    for (size_t i = 0; i < NUMBER_OF_OPTIONS; i++) {
        // Find the option with the matching key
        if (strcmp(options[i].key, key) == 0) {
            // .. and find where the value will bee stored
            void *dst = (char *)cfg + options[i].offset;
            char *end;

            switch (options[i].type) {
                case CFG_INT: {
                    long v = strtol(val, &end, 10);
                    // Check if the conversion was successful before writing to destination
                    if (end == val || *end != '\0') { return 1; }
                    *(int *)dst = (int)v;
                    return 0;
                }
                case CFG_FLOAT: {
                    float v = strtof(val, &end);
                    // Check if the conversion was successful before writing to destination
                    if (end == val || *end != '\0') { return 1; }
                    *(float *)dst = v;
                    return 0;
                }
                case CFG_STRING:
                    free(*(char **)dst);
                    *(char **)dst = _strdup(val);
                    return 0;
                default:
                    break;
            }
        }
    }
    return 1; // Key not found
}

void setConfigDefaults(struct Config *cfg) {
    for (size_t i = 0; i < NUMBER_OF_OPTIONS; i++) {
        setConfigValue(cfg, options[i].key, options[i].defaultValue);
    }
}

int loadConfigFile(struct Config *cfg, const char* filename) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) { return 1; }

    char line[MAX_LINE_LENGTH];
    while (fgets(line, sizeof(line), file) != NULL) {
        // Split the line at the first '=', and trim the spaces and newline from both halves
        char *equalsIndex = strchr(line, '=');
        if (equalsIndex == NULL) { continue; }
        *equalsIndex = '\0';
        char *key = trim(line);
        char *value = trim(equalsIndex + 1);
        if (setConfigValue(cfg, key, value) != 0) {
            printf("Warning: Ignoring config option %s=%s, as the option is unknown or the value is invalid.\n", key, value);
        }
    }

    fclose(file);
    return 0;
}

int saveConfigFile(const struct Config *cfg, const char* filename) {
    FILE *file = fopen(filename, "w");
    if (file == NULL) { return 1; }

    for (size_t i = 0; i < NUMBER_OF_OPTIONS; i++) {
        const void *src = (const char *)cfg + options[i].offset;
        switch (options[i].type) {
            case CFG_INT:
                fprintf(file, "%s=%d\n", options[i].key, *(const int *)src);
                break;
            case CFG_FLOAT:
                fprintf(file, "%s=%g\n", options[i].key, *(const float *)src);
                break;
            case CFG_STRING: {
                const char *value = *(char * const *)src;
                if (value != NULL) { fprintf(file, "%s=%s\n", options[i].key, value); }
                break;
            }
            default:
                break;
        }
    }

    fclose(file);
    return 0;
}

void freeConfig(struct Config *cfg) {
    for (size_t i = 0; i < NUMBER_OF_OPTIONS; i++) {
        if (options[i].type == CFG_STRING) {
            char **value = (char **)((char *)cfg + options[i].offset);
            free(*value);
            *value = NULL;
        }
    }
}