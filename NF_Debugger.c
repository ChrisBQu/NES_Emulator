#include "NF_6502.h"
#include "NF_PPU.h"
#include <stdio.h>
#include <stdint.h>


// Get the number of bytes that an opcode uses
uint8_t getAddressModeToByteCount(ADDRESS_MODE_6502 value) {
	int8_t byte_size_arr[14] = { 1, 2, 2, 1, 2, 2, 2, 3, 3, 3, 3, 2, 2, 0 };
	if (value < AM_XXX) { return byte_size_arr[value]; }
	return 0;
}

// Turn an opcode in OPCODE_6502 format to a string
const char* opcodeToString(OPCODE_6502 value) {
	const char* opcode_string_arr[57] = {
		"ADC", "AND", "ASL", "BCC", "BCS", "BEQ", "BIT", "BMI", "BNE", "BPL", "BRK", "BVC", "BVS", "CLC",
		"CLD", "CLI", "CLV", "CMP", "CPX", "CPY", "DEC", "DEX", "DEY", "EOR", "INC", "INX", "INY", "JMP",
		"JSR", "LDA", "LDX", "LDY", "LSR", "NOP", "ORA", "PHA", "PHP", "PLA", "PLP", "ROL", "ROR", "RTI",
		"RTS", "SBC", "SEC", "SED", "SEI", "STA", "STX", "STY", "TAX", "TAY", "TSX", "TXA", "TXS", "TYA", "XXX"
	};
	return opcode_string_arr[value];
};


char builtStringBuffer[50];

// Build strings for debugging such that they match the format output by nestest.nes
const char* buildFetchString(struct Processor *CPU) {


	ADDRESS_MODE_6502 addr_mode = charToAddressModeArray[NF_readMemory(CPU->bus, CPU->last_pc)];
	OPCODE_6502 opcode = charToOpcodeArray[NF_readMemory(CPU->bus, CPU->last_pc)];
	//printf("last_pc: %02X, AM_MODE: %d\n", CPU->last_pc, addr_mode);
	uint16_t lo;
	uint16_t hi;
	switch (addr_mode) {
	case AM_ABS:
		if (opcode == OP_JSR || opcode == OP_JMP) { sprintf(builtStringBuffer, "$%02X%02X                      ", NF_readMemory(CPU->bus, CPU->last_pc + 2), NF_readMemory(CPU->bus, CPU->last_pc + 1)); }
		else { 
			hi = NF_readMemory(CPU->bus, CPU->last_pc + 2);
			lo  = NF_readMemory(CPU->bus, CPU->last_pc + 1);
			if (opcode == OP_STX || opcode == OP_STY || opcode == OP_STA) { sprintf(builtStringBuffer, "$%02X%02X = %02X                 ", hi, lo, NF_readMemory(CPU->bus, (hi << 8) | lo)); }
			else { sprintf(builtStringBuffer, "$%02X%02X = %02X                 ", hi, lo, NF_readMemory(CPU->bus, (hi << 8) | lo)); }
		}
		break;
	case AM_IND:
		sprintf(builtStringBuffer, "($%02X%02X) = %04X             ", NF_readMemory(CPU->bus, CPU->last_pc + 2), NF_readMemory(CPU->bus, CPU->last_pc + 1), CPU->fetched_address);
		break;
	case AM_IMM:
		sprintf(builtStringBuffer, "#$%02X                       ", NF_readMemory(CPU->bus, CPU->last_pc + 1));
		break;
	case AM_ZPG:
		sprintf(builtStringBuffer, "$%02X = %02X                   ", NF_readMemory(CPU->bus, CPU->last_pc + 1), CPU->fetched);
		break;
	case AM_ZPX:
		sprintf(builtStringBuffer, "$%02X,X @ %02X = %02X            ", NF_readMemory(CPU->bus, CPU->last_pc + 1), (uint8_t)((uint8_t)CPU->X + (uint8_t)NF_readMemory(CPU->bus, CPU->last_pc + 1)), CPU->fetched);
		break;
	case AM_ZPY:
		sprintf(builtStringBuffer, "$%02X,Y @ %02X = %02X            ", NF_readMemory(CPU->bus, CPU->last_pc + 1), (uint8_t)((uint8_t)CPU->Y + (uint8_t)NF_readMemory(CPU->bus, CPU->last_pc + 1)), CPU->fetched);
		break;
	case AM_INX:
		sprintf(builtStringBuffer, "($%02X,X) @ %02X = %04X = %02X   ", NF_readMemory(CPU->bus, CPU->last_pc + 1), (uint8_t)(CPU->X + NF_readMemory(CPU->bus, CPU->last_pc + 1)), CPU->fetched_address, CPU->fetched);
		break;
	case AM_INY:
		sprintf(builtStringBuffer, "($%02X),Y = %04X @ %04X = %02X ", NF_readMemory(CPU->bus, CPU->last_pc + 1), (uint16_t)(CPU->fetched_address - CPU->Y), CPU->fetched_address, CPU->fetched);
		break;
	case AM_REL:
		sprintf(builtStringBuffer, "$%04X                      ", CPU->fetched_address);
		break;
	case AM_ABY:
		sprintf(builtStringBuffer, "$%04X,Y @ %04X = %02X        ", (uint16_t)(CPU->fetched_address - CPU->Y), CPU->fetched_address, CPU->fetched);
		break;
	case AM_ABX:
		sprintf(builtStringBuffer, "$%04X,X @ %04X = %02X        ", (uint16_t)(CPU->fetched_address - CPU->X), CPU->fetched_address, CPU->fetched);
		break;
	case AM_ACC:
		sprintf(builtStringBuffer, "A                          ");
		break;
	case AM_IMP:
		sprintf(builtStringBuffer, "                           ");
		break;
	default:
		sprintf(builtStringBuffer, "                           ");
		break;
	}
	return builtStringBuffer;
}

// Print a line showing the debug information of the Processor and PPU in format:
//
// C000  4C F5 C5  JMP $C5F5                       A : 00 X : 00 Y : 00 P : 24 SP : FD PPU : 0, 21 CYC : 7
// C5F5  A2 00     LDX #$00                        A : 00 X : 00 Y : 00 P : 24 SP : FD PPU : 0, 30 CYC : 10
// C5F7  86 00     STX $00 = 00                    A : 00 X : 00 Y : 00 P : 26 SP : FD PPU : 0, 36 CYC : 12
// C5F9  86 10     STX $10 = 00                    A : 00 X : 00 Y : 00 P : 26 SP : FD PPU : 0, 45 CYC : 15
// C5FB  86 11     STX $11 = 00                    A : 00 X : 00 Y : 00 P : 26 SP : FD PPU : 0, 54 CYC : 18
// etc.
//
void printToDebugFile(FILE* log, struct Processor* CPU) {
	int bytecount = getAddressModeToByteCount(CPU->addr_mode);
	if (bytecount == 1) {
		fprintf(log, "%04X  %02X        %s %s A:%02X X:%02X Y:%02X P:%02X SP:%02X PPU:%3d,%3d CYC:", CPU->last_pc, NF_readMemory(CPU->bus, CPU->last_pc),
			opcodeToString(CPU->opcode), buildFetchString(CPU), CPU->A, CPU->X, CPU->Y, CPU->P, CPU->SP, CPU->bus->ConnectedPPU->scanline, CPU->bus->ConnectedPPU->cycle);
	}
	else if (bytecount == 2) {
		fprintf(log, "%04X  %02X %02X     %s %s A:%02X X:%02X Y:%02X P:%02X SP:%02X PPU:%3d,%3d CYC:", CPU->last_pc, NF_readMemory(CPU->bus, CPU->last_pc),
			NF_readMemory(CPU->bus, CPU->last_pc + 1), opcodeToString(CPU->opcode), buildFetchString(CPU), CPU->A, CPU->X, CPU->Y, CPU->P,
			CPU->SP, CPU->bus->ConnectedPPU->scanline, CPU->bus->ConnectedPPU->cycle);
	}
	else if (bytecount == 3) {
		fprintf(log, "%04X  %02X %02X %02X  %s %s A:%02X X:%02X Y:%02X P:%02X SP:%02X PPU:%3d,%3d CYC:", CPU->last_pc, NF_readMemory(CPU->bus, CPU->last_pc),
			NF_readMemory(CPU->bus, CPU->last_pc + 1), NF_readMemory(CPU->bus, CPU->last_pc + 2), opcodeToString(CPU->opcode), buildFetchString(CPU), CPU->A,
			CPU->X, CPU->Y, CPU->P, CPU->SP, CPU->bus->ConnectedPPU->scanline, CPU->bus->ConnectedPPU->cycle);
	}
}