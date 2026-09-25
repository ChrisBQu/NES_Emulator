#include "NF_APU.h"
#include "NF_6502.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Important terminology used in this file:
// - Duty: the fraction of each wave cycle the sound is on rather than off, which changes the tone.
// - Sweep: an automatic pitch slide that makes the note go up or down over time.
// - Envelope: the note's volume, which either stays constant or fades out over time.
// - Period: a timer value that sets the note's pitch, where a smaller period means a higher note.

// Writing a channel's 4th register ($4003, $4007, $400B, or $400F) loads the counter with
// an entry chosen by bits 3-7 of the value. The table is defined by the hardware.
// See: https://www.nesdev.org/wiki/APU_Length_Counter
static const uint8_t length_table[32] = {
	10, 254, 20,  2, 40,  4, 80,  6, 160,  8, 60, 10, 14, 12, 26, 14,
	12,  16, 24, 18, 48, 20, 96, 22, 192, 24, 72, 26, 16, 28, 32, 30
};

// Waveform shapes for the pulse channels. Each row is an 8-step pattern: 1 outputs the channel's volume, 0 outputs silence.
// The row is determined by bits 6-7 of $4000 (for pulse 1) or $4004 (for pulse 2).
// 75% sounds identical to 25%, because it is the same wave inverted.
// See: https://www.nesdev.org/wiki/APU_Pulse
static const uint8_t pulse_duty_table[4][8] = {
	{ 0, 1, 0, 0, 0, 0, 0, 0 }, 
	{ 0, 1, 1, 0, 0, 0, 0, 0 },
	{ 0, 1, 1, 1, 1, 0, 0, 0 }, 
	{ 1, 0, 0, 1, 1, 1, 1, 1 },
};

// This table defines the shape of the triangle wave. It is a 32-step pattern that decreases from 15 to 0,
// then goes back up to 15.
// See: https://www.nesdev.org/wiki/APU_Triangle
static const uint8_t triangle_table[32] = {
	15, 14, 13, 12, 11, 10,  9,  8,  7,  6,  5,  4,  3,  2,  1,  0,
	 0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15
};

// This table defines the shape of the noise wave, as defined by the hardware.
// See: https://www.nesdev.org/wiki/APU_Noise
static const uint16_t noise_period_table[16] = {
	4, 8, 16, 32, 64, 96, 128, 160, 202, 254, 380, 508, 762, 1016, 2034, 4068
};

// The number of CPU cycles between each change in the DMC's output level, chosen by bits 0-3 of $4010.
// A smaller value plays the sample faster and higher pitched.
// See: https://www.nesdev.org/wiki/APU_DMC
static const uint16_t dmc_rate_table[16] = {
	428, 380, 340, 320, 286, 254, 226, 214, 190, 160, 142, 128, 106, 84, 72, 54
};

// CPU cycles the CPU is paused while the DMC fetches a sample byte (can be up to 4)
#define DMC_FETCH_STALL_CYCLES 4

// Frame counter step timings, in CPU cycles.
// CPU cycle = 2 * APU cycle + 1 for writes, and 2 * APU cycle + 0 for reads.
// See: https://www.nesdev.org/wiki/APU_Frame_Counter
// Note:If bit 7 of $4017 is set, the frame counter will run at 5-step mode.
#define FRAME_STEP_1 7457
#define FRAME_STEP_2 14913
#define FRAME_STEP_3 22371
#define FRAME_STEP_4 29829
#define FRAME_STEP_5 37281

// Cutoff of the high-pass filter that removes the DC offset
#define HIGHPASS_CUTOFF_HZ 90.0

// Constructor
struct AudioProcessingUnit* NF_initAPU() {
	struct AudioProcessingUnit* apu = malloc(sizeof(struct AudioProcessingUnit));
	if (apu == NULL) {
		printf("Error: Could not create APU object. Out of memory?\n");
		return NULL;
	}
	memset(apu, 0, sizeof(struct AudioProcessingUnit));

	apu->pulse1.is_pulse1 = true;
	apu->noise.shift_register = 1;  // The shift register is loaded with 1 on power-up
	apu->noise.timer_period = noise_period_table[0];
	apu->dmc.timer_period = dmc_rate_table[0];
	apu->dmc.sample_address = 0xC000;
	apu->dmc.sample_length = 1;
	apu->dmc.sample_buffer_empty = true;
	apu->dmc.bits_remaining = 8;
	apu->dmc.silence = true;
	NF_APU_setSampleRate(apu, 44100.0);
	return apu;
}

// Set the output sample rate, and recompute the high-pass filter coefficient for that rate
void NF_APU_setSampleRate(struct AudioProcessingUnit* apu, double sample_rate) {
	apu->sample_rate = sample_rate;
	apu->sample_clock = 0.0;
	apu->sample_sum = 0.0f;
	apu->sample_count = 0;

	// First order high-pass: a = RC / (RC + dt)
    // See: https://en.wikipedia.org/wiki/High-pass_filter
	double resistance_times_capacitance = 1.0 / (2.0 * 3.14159265358979 * HIGHPASS_CUTOFF_HZ);
	double delta_time = 1.0 / sample_rate;
	apu->highpass_coefficient = (float)(resistance_times_capacitance / (resistance_times_capacitance + delta_time));
	apu->highpass_prev_in = 0.0f;
	apu->highpass_prev_out = 0.0f;
}

// Tick the envelope, which either restarts at 15 or fades the volume down one level each time its divider expires
// See: https://www.nesdev.org/wiki/APU_Envelope
static void tickEnvelope(struct NF_APU_Envelope* env) {
	if (env->start) {
		env->start = false;
		env->decay = 15;
		env->divider = env->volume;
	}
	else if (env->divider == 0) {
		env->divider = env->volume;
		if (env->decay > 0) { env->decay--; }
		else if (env->loop) { env->decay = 15; }
	}
	else { env->divider--; }
}

// The envelope's current volume: either the constant volume, or the decaying level
static uint8_t envelopeOutput(struct NF_APU_Envelope* env) {
	return env->constant ? env->volume : env->decay;
}

// The period the sweep unit is trying to change to. Pulse 1 subtracts an extra 1 when negating
static uint16_t sweepTargetPeriod(struct NF_APU_Pulse* pulse) {
	uint16_t change = pulse->timer_period >> pulse->sweep_shift;
	if (pulse->sweep_negate) {
		int target = (int)pulse->timer_period - change - (pulse->is_pulse1 ? 1 : 0);
		return target < 0 ? 0 : (uint16_t)target;
	}
	return pulse->timer_period + change;
}

// The sweep unit mutes the channel if the period is too low, or if the target period would overflow
static bool pulseMuted(struct NF_APU_Pulse* pulse) {
	return pulse->timer_period < 8 || sweepTargetPeriod(pulse) > 0x7FF;
}

// Tick the sweep unit, which slides the pulse channel's pitch up or down by periodically changing its period
// See: https://www.nesdev.org/wiki/APU_Sweep
static void tickSweep(struct NF_APU_Pulse* pulse) {
	if (pulse->sweep_divider == 0 && pulse->sweep_enabled && pulse->sweep_shift > 0 && !pulseMuted(pulse)) {
		pulse->timer_period = sweepTargetPeriod(pulse);
	}
	if (pulse->sweep_divider == 0 || pulse->sweep_reload) {
		pulse->sweep_divider = pulse->sweep_period;
		pulse->sweep_reload = false;
	}
	else { pulse->sweep_divider--; }
}

// Count down the pulse timer, and step to the next position in the duty pattern each time it expires
// See: https://www.nesdev.org/wiki/APU_Pulse
static void tickPulseTimer(struct NF_APU_Pulse* pulse) {
	if (pulse->timer == 0) {
		pulse->timer = pulse->timer_period;
		pulse->sequence_pos = (pulse->sequence_pos + 1) & 0x07;
	}
	else { pulse->timer--; }
}

// The pulse channel's current output level: the envelope volume on a 1 step of its duty pattern, otherwise 0
static uint8_t pulseOutput(struct NF_APU_Pulse* pulse) {
	if (pulse->length == 0 || pulseMuted(pulse)) { return 0; }
	if (pulse_duty_table[pulse->duty][pulse->sequence_pos] == 0) { return 0; }
	return envelopeOutput(&pulse->envelope);
}

// Handle a write to one of a pulse channel's 4 registers (reg 0-3), setting its duty, envelope, sweep or period
static void writePulse(struct NF_APU_Pulse* pulse, uint8_t reg, uint8_t data) {
	switch (reg) {
	case 0:
		pulse->duty = data >> 6;
		pulse->envelope.loop = (data & 0x20) != 0;
		pulse->envelope.constant = (data & 0x10) != 0;
		pulse->envelope.volume = data & 0x0F;
		break;
	case 1:
		pulse->sweep_enabled = (data & 0x80) != 0;
		pulse->sweep_period = (data >> 4) & 0x07;
		pulse->sweep_negate = (data & 0x08) != 0;
		pulse->sweep_shift = data & 0x07;
		pulse->sweep_reload = true;
		break;
	case 2:
		pulse->timer_period = (pulse->timer_period & 0x0700) | data;
		break;
	case 3:
		pulse->timer_period = (pulse->timer_period & 0x00FF) | ((uint16_t)(data & 0x07) << 8);
		if (pulse->enabled) { pulse->length = length_table[data >> 3]; }
		pulse->sequence_pos = 0;
		pulse->envelope.start = true;
		break;
	}
}

// Reload or count down the triangle's linear counter, which gives it a finer-grained duration than the length counter
static void tickLinearCounter(struct NF_APU_Triangle* tri) {
	if (tri->linear_reload_flag) { tri->linear_counter = tri->linear_reload; }
	else if (tri->linear_counter > 0) { tri->linear_counter--; }
	if (!tri->control) { tri->linear_reload_flag = false; }
}

// Count down the triangle timer, and step through the waveform each time it expires (if both counters are non-zero)
static void tickTriangleTimer(struct NF_APU_Triangle* tri) {
	if (tri->timer == 0) {
		tri->timer = tri->timer_period;
		// Very low periods produce ultrasonic frequencies that just pop, so hold the sequencer instead
		if (tri->length > 0 && tri->linear_counter > 0 && tri->timer_period >= 2) {
			tri->sequence_pos = (tri->sequence_pos + 1) & 0x1F;
		}
	}
	else { tri->timer--; }
}

// The triangle has no volume control. When silenced it holds its last output level
static uint8_t triangleOutput(struct NF_APU_Triangle* tri) {
	return triangle_table[tri->sequence_pos];
}

// Count down the noise timer, and shift a new pseudo-random bit into the shift register each time it expires
// See: https://www.nesdev.org/wiki/APU_Noise
static void tickNoiseTimer(struct NF_APU_Noise* noise) {
	if (noise->timer == 0) {
		noise->timer = noise->timer_period;
		uint16_t other_bit = noise->mode ? (noise->shift_register >> 6) : (noise->shift_register >> 1);
		uint16_t feedback = (noise->shift_register ^ other_bit) & 0x01;
		noise->shift_register = (noise->shift_register >> 1) | (feedback << 14);
	}
	else { noise->timer--; }
}

// The noise channel's current output level: silent when bit 0 of the shift register is set or the length counter is 0
static uint8_t noiseOutput(struct NF_APU_Noise* noise) {
	if (noise->length == 0 || (noise->shift_register & 0x01)) { return 0; }
	return envelopeOutput(&noise->envelope);
}

// Start playing the sample from the beginning
static void restartDMCSample(struct NF_APU_DMC* dmc) {
	dmc->current_address = dmc->sample_address;
	dmc->bytes_remaining = dmc->sample_length;
}

// If the sample buffer is empty and the sample isn't finished, fetch the next sample byte into it, pausing the CPU
// See: https://www.nesdev.org/wiki/APU_DMC
static void fillDMCSampleBuffer(struct AudioProcessingUnit* apu) {
	struct NF_APU_DMC* dmc = &apu->dmc;
	if (!dmc->sample_buffer_empty || dmc->bytes_remaining == 0) { return; }

	dmc->sample_buffer = NF_readMemory(apu->bus, dmc->current_address);
	dmc->sample_buffer_empty = false;
	apu->bus->ConnectedProcessor->cycles += DMC_FETCH_STALL_CYCLES;

	// The address wraps around to $8000 after $FFFF
	dmc->current_address = (dmc->current_address == 0xFFFF) ? 0x8000 : dmc->current_address + 1;

	dmc->bytes_remaining--;
	if (dmc->bytes_remaining == 0) {
		if (dmc->loop) { restartDMCSample(dmc); }
		else if (dmc->irq_enabled) { dmc->irq = true; }
	}
}

// Count down the DMC timer, and each time it expires move the output level up or down by 2 based on the next sample bit
// See: https://www.nesdev.org/wiki/APU_DMC
static void tickDMCTimer(struct NF_APU_DMC* dmc) {
	if (dmc->timer > 0) {
		dmc->timer--;
		return;
	}
	dmc->timer = dmc->timer_period - 1;

	// A 1 bit raises the level and a 0 bit lowers it, unless that would leave the 0-127 range
	if (!dmc->silence) {
		if (dmc->shift_register & 0x01) {
			if (dmc->output_level <= 125) { dmc->output_level += 2; }
		}
		else if (dmc->output_level >= 2) { dmc->output_level -= 2; }
	}
	dmc->shift_register >>= 1;

	// After 8 bits, start on the next byte, or go silent if the memory reader hasn't fetched one
	dmc->bits_remaining--;
	if (dmc->bits_remaining == 0) {
		dmc->bits_remaining = 8;
		if (dmc->sample_buffer_empty) { dmc->silence = true; }
		else {
			dmc->silence = false;
			dmc->shift_register = dmc->sample_buffer;
			dmc->sample_buffer_empty = true;
		}
	}
}

// This will run on all frame steps
static void tickQuarterFrame(struct AudioProcessingUnit* apu) {
	tickEnvelope(&apu->pulse1.envelope);
	tickEnvelope(&apu->pulse2.envelope);
	tickEnvelope(&apu->noise.envelope);
	tickLinearCounter(&apu->triangle);
}

// This will run on frame steps 2, 4, and 5
static void tickHalfFrame(struct AudioProcessingUnit* apu) {
	if (apu->pulse1.length > 0 && !apu->pulse1.envelope.loop) { apu->pulse1.length--; }
	if (apu->pulse2.length > 0 && !apu->pulse2.envelope.loop) { apu->pulse2.length--; }
	if (apu->triangle.length > 0 && !apu->triangle.control) { apu->triangle.length--; }
	if (apu->noise.length > 0 && !apu->noise.envelope.loop) { apu->noise.length--; }
	tickSweep(&apu->pulse1);
	tickSweep(&apu->pulse2);
}

// Advance the frame counter by one CPU cycle, firing quarter and half frame ticks at the FRAME_STEP timings
static void tickFrameCounter(struct AudioProcessingUnit* apu) {
	apu->frame_cycle++;

	switch (apu->frame_cycle) {
	case FRAME_STEP_1: tickQuarterFrame(apu); break;
	case FRAME_STEP_2: tickQuarterFrame(apu); tickHalfFrame(apu); break;
	case FRAME_STEP_3: tickQuarterFrame(apu); break;
	case FRAME_STEP_4:
		if (!apu->frame_mode_5step) {
			tickQuarterFrame(apu);
			tickHalfFrame(apu);
			if (!apu->irq_inhibit) { apu->frame_irq = true; }
			apu->frame_cycle = 0;
		}
		break;
	case FRAME_STEP_5:
		// Only reached in 5-step mode
		tickQuarterFrame(apu);
		tickHalfFrame(apu);
		apu->frame_cycle = 0;
		break;
	}
}

// Non-linear mixing approximation from the NESdev wiki. Output is in the range [0.0, ~1.0]
// See: https://www.nesdev.org/wiki/APU_Mixer
static float mix(struct AudioProcessingUnit* apu) {
	uint8_t p1 = pulseOutput(&apu->pulse1);
	uint8_t p2 = pulseOutput(&apu->pulse2);
	uint8_t t = triangleOutput(&apu->triangle);
	uint8_t n = noiseOutput(&apu->noise);
	uint8_t d = apu->dmc.output_level;

	float pulse_out = 0.0f;
	if (p1 + p2 > 0) { pulse_out = 95.88f / (8128.0f / (float)(p1 + p2) + 100.0f); }

	float tnd_out = 0.0f;
	float tnd_sum = (float)t / 8227.0f + (float)n / 12241.0f + (float)d / 22638.0f;
	if (tnd_sum > 0.0f) { tnd_out = 159.79f / (1.0f / tnd_sum + 100.0f); }

	return pulse_out + tnd_out;
}

// Average the mixer output over the CPU cycles making up one output sample, then send it out through the bus
static void generateSample(struct AudioProcessingUnit* apu) {
	if (apu->bus == NULL || apu->bus->audioOutFunc == NULL || apu->sample_rate <= 0.0) { return; }

	apu->sample_sum += mix(apu);
	apu->sample_count++;
	apu->sample_clock += apu->sample_rate;

	if (apu->sample_clock >= NF_APU_CPU_CLOCK_RATE) {
		apu->sample_clock -= NF_APU_CPU_CLOCK_RATE;

		float in = apu->sample_sum / (float)apu->sample_count;
		float out = apu->highpass_coefficient * (apu->highpass_prev_out + in - apu->highpass_prev_in);
		apu->highpass_prev_in = in;
		apu->highpass_prev_out = out;

		apu->sample_sum = 0.0f;
		apu->sample_count = 0;
		apu->bus->audioOutFunc(out);
	}
}

// Advance the APU by one CPU cycle: step the frame counter and channel timers, then produce audio output
void NF_APU_tickClock(struct AudioProcessingUnit* apu) {
	tickFrameCounter(apu);

	// The triangle, noise and DMC timers run at the CPU rate, the pulse timers at half of it
	tickTriangleTimer(&apu->triangle);
	tickNoiseTimer(&apu->noise);
	tickDMCTimer(&apu->dmc);
	fillDMCSampleBuffer(apu);
	if (apu->odd_cycle) {
		tickPulseTimer(&apu->pulse1);
		tickPulseTimer(&apu->pulse2);
	}
	apu->odd_cycle = !apu->odd_cycle;

	generateSample(apu);
}

// Handle a CPU write to one of the APU registers, updating the channel or frame counter it controls
void NF_APU_writeRegister(struct AudioProcessingUnit* apu, uint16_t address, uint8_t data) {
	switch (address) {

	// Pulse 1
	case 0x4000: case 0x4001: case 0x4002: case 0x4003:
		writePulse(&apu->pulse1, address & 0x03, data);
		break;

    // Pulse 2
	case 0x4004: case 0x4005: case 0x4006: case 0x4007:
		writePulse(&apu->pulse2, address & 0x03, data);
		break;

	// Triangle
	case 0x4008:
		apu->triangle.control = (data & 0x80) != 0;
		apu->triangle.linear_reload = data & 0x7F;
		break;
	case 0x4009: 
        break;
	case 0x400A:
		apu->triangle.timer_period = (apu->triangle.timer_period & 0x0700) | data;
		break;
	case 0x400B:
		apu->triangle.timer_period = (apu->triangle.timer_period & 0x00FF) | ((uint16_t)(data & 0x07) << 8);
		if (apu->triangle.enabled) { apu->triangle.length = length_table[data >> 3]; }
		apu->triangle.linear_reload_flag = true;
		break;

	// Noise
	case 0x400C:
		apu->noise.envelope.loop = (data & 0x20) != 0;
		apu->noise.envelope.constant = (data & 0x10) != 0;
		apu->noise.envelope.volume = data & 0x0F;
		break;
	case 0x400D: 
        break;
	case 0x400E:
		apu->noise.mode = (data & 0x80) != 0;
		apu->noise.timer_period = noise_period_table[data & 0x0F];
		break;
	case 0x400F:
		if (apu->noise.enabled) { apu->noise.length = length_table[data >> 3]; }
		apu->noise.envelope.start = true;
		break;

	// DMC
	case 0x4010:
		apu->dmc.irq_enabled = (data & 0x80) != 0;
		apu->dmc.loop = (data & 0x40) != 0;
		apu->dmc.timer_period = dmc_rate_table[data & 0x0F];
		if (!apu->dmc.irq_enabled) { apu->dmc.irq = false; }
		break;
	case 0x4011:
		apu->dmc.output_level = data & 0x7F;
		break;
	case 0x4012:
		apu->dmc.sample_address = 0xC000 + ((uint16_t)data * 64);
		break;
	case 0x4013:
		apu->dmc.sample_length = ((uint16_t)data * 16) + 1;
		break;

	// Status: enable/disable channels. Disabling a channel immediately silences it
	case 0x4015:
		apu->pulse1.enabled = (data & 0x01) != 0;
		apu->pulse2.enabled = (data & 0x02) != 0;
		apu->triangle.enabled = (data & 0x04) != 0;
		apu->noise.enabled = (data & 0x08) != 0;
		if (!apu->pulse1.enabled) { apu->pulse1.length = 0; }
		if (!apu->pulse2.enabled) { apu->pulse2.length = 0; }
		if (!apu->triangle.enabled) { apu->triangle.length = 0; }
		if (!apu->noise.enabled) { apu->noise.length = 0; }

		// Disabling the DMC stops the sample. Enabling it starts the sample, unless one is already playing
		if (!(data & 0x10)) { apu->dmc.bytes_remaining = 0; }
		else if (apu->dmc.bytes_remaining == 0) { restartDMCSample(&apu->dmc); }
		apu->dmc.irq = false;
		break;

	// Frame counter: resets the sequence, and 5-step mode ticks everything immediately
	case 0x4017:
		apu->frame_mode_5step = (data & 0x80) != 0;
		apu->irq_inhibit = (data & 0x40) != 0;
		if (apu->irq_inhibit) { apu->frame_irq = false; }
		apu->frame_cycle = 0;
		if (apu->frame_mode_5step) {
			tickQuarterFrame(apu);
			tickHalfFrame(apu);
		}
		break;
	}
}

// Read the status register ($4015): which channels are still playing, plus the frame IRQ flag, which reading clears
uint8_t NF_APU_readStatus(struct AudioProcessingUnit* apu) {
	uint8_t status = 0;
	if (apu->pulse1.length > 0) { status |= 0x01; }
	if (apu->pulse2.length > 0) { status |= 0x02; }
	if (apu->triangle.length > 0) { status |= 0x04; }
	if (apu->noise.length > 0) { status |= 0x08; }
	if (apu->dmc.bytes_remaining > 0) { status |= 0x10; }
	if (apu->frame_irq) { status |= 0x40; }
	if (apu->dmc.irq) { status |= 0x80; }
	apu->frame_irq = false;
	return status;
}
