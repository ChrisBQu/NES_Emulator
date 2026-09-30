#include "Mapper.h"
#include <stddef.h>

// Hooks that aren't listed are NULL
struct Mapper MapperList[MAPPER_COUNT] = {
	[0] = {
		.init = Mapper0_init,
		.readPRG_ROM = Mapper0_readPRG_ROM, .writePRG = Mapper0_writePRG,
		.readPRG_RAM = Mapper0_readPRG_RAM, .writePRG_RAM = Mapper0_writePRG_RAM,
		.readCHR_ROM = Mapper0_readCHR_ROM, .writeCHR = Mapper0_writeCHR
	},
	[1] = {
		.state_size = sizeof(struct Mapper1_State),
		.init = Mapper1_init,
		.readPRG_ROM = Mapper1_readPRG_ROM, .writePRG = Mapper1_writePRG,
		.readPRG_RAM = Mapper1_readPRG_RAM, .writePRG_RAM = Mapper1_writePRG_RAM,
		.readCHR_ROM = Mapper1_readCHR_ROM, .writeCHR = Mapper1_writeCHR
	},
	[2] = {
		.state_size = sizeof(struct Mapper2_State),
		.init = Mapper2_init,
		.readPRG_ROM = Mapper2_readPRG_ROM, .writePRG = Mapper2_writePRG,
		.readPRG_RAM = Mapper0_readPRG_RAM, .writePRG_RAM = Mapper0_writePRG_RAM,
		.readCHR_ROM = Mapper2_readCHR_ROM, .writeCHR = Mapper2_writeCHR
	},
	[3] = {
		.state_size = sizeof(struct Mapper3_State),
		.init = Mapper3_init,
		.readPRG_ROM = Mapper0_readPRG_ROM, .writePRG = Mapper3_writePRG,
		.readPRG_RAM = Mapper0_readPRG_RAM, .writePRG_RAM = Mapper0_writePRG_RAM,
		.readCHR_ROM = Mapper3_readCHR_ROM, .writeCHR = Mapper3_writeCHR
	},
	[4] = {
		.state_size = sizeof(struct Mapper4_State),
		.init = Mapper4_init,
		.readPRG_ROM = Mapper4_readPRG_ROM, .writePRG = Mapper4_writePRG,
		.readPRG_RAM = Mapper0_readPRG_RAM, .writePRG_RAM = Mapper0_writePRG_RAM,
		.readCHR_ROM = Mapper4_readCHR_ROM, .writeCHR = Mapper4_writeCHR,
		.notifyPPUAddress = Mapper4_notifyPPUAddress
	},
	[5] = {
		.state_size = sizeof(struct Mapper5_State),
		.init = Mapper5_init,
		.readPRG_ROM = Mapper5_readPRG_ROM, .writePRG = Mapper5_writePRG,
		.readPRG_RAM = Mapper5_readPRG_RAM, .writePRG_RAM = Mapper5_writePRG_RAM,
		.readCHR_ROM = Mapper5_readCHR_ROM, .writeCHR = Mapper5_writeCHR,
		.notifyPPUAddress = Mapper5_notifyPPUAddress,
		.notifyPPURegisterWrite = Mapper5_notifyPPURegisterWrite,
		.readExpansion = Mapper5_readExpansion, .writeExpansion = Mapper5_writeExpansion,
		.readNametable = Mapper5_readNametable, .writeNametable = Mapper5_writeNametable,
		.tick = Mapper5_tick
	}
};