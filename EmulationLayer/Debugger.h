#ifndef NES_DEBUGGER_H
#define NES_DEBUGGER_H

#include <stdlib.h>

#define DEBUG_ENABLED 0

#include "6502.h"
#include <stdlib.h>

uint8_t getAddressModeToByteCount(ADDRESS_MODE_6502 value);
const char* opcodeToString(OPCODE_6502 value);
const char* buildFetchString(struct Processor* CPU);
void printToDebugFile(FILE* log, struct Processor* CPU);

#endif