#ifndef NF_DEBUGGER_H
#define NF_DEBUGGER_H

#include "NF_6502.h"
#include <stdlib.h>

uint8_t getAddressModeToByteCount(ADDRESS_MODE_6502 value);
const char* opcodeToString(OPCODE_6502 value);
const char* buildFetchString(struct Processor* CPU);

#endif