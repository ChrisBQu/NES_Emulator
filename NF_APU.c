#include "NF_APU.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Values loaded into a length counter, indexed by the top 5 bits of the channel's 4th register
static const uint8_t length_table[32] = {
	10, 254, 20,  2, 40,  4, 80,  6, 160,  8, 60, 10, 14, 12, 26, 14,
	12,  16, 24, 18, 48, 20, 96, 22, 192, 24, 72, 26, 16, 28, 32, 30
};

// The 4 pulse duty cycles (12.5%, 25%, 50%, 25% negated)
static const uint8_t duty_table[4][8] = {
	{ 0, 1, 0, 0, 0, 0, 0, 0 },
	{ 0, 1, 1, 0, 0, 0, 0, 0 },
	{ 0, 1, 1, 1, 1, 0, 0, 0 },
	{ 1, 0, 0, 1, 1, 1, 1, 1 }
};

// The triangle steps down 15 -> 0, then back up 0 -> 15
static const uint8_t triangle_table[32] = {
	15, 14, 13, 12, 11, 10,  9,  8,  7,  6,  5,  4,  3,  2,  1,  0,
	 0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15
};

// Noise timer periods in CPU cycles (NTSC)
static const uint16_t noise_period_table[16] = {
	4, 8, 16, 32, 64, 96, 128, 160, 202, 254, 380, 508, 762, 1016, 2034, 4068
};

// Frame counter step timings, in CPU cycles
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
	NF_APU_setSampleRate(apu, 44100.0);
	return apu;
}

void NF_APU_setSampleRate(struct AudioProcessingUnit* apu, double sample_rate) {
	apu->sample_rate = sample_rate;
	apu->sample_clock = 0.0;
	apu->sample_sum = 0.0f;
	apu->sample_count = 0;

	// First order high-pass: a = RC / (RC + dt)
	double rc = 1.0 / (2.0 * 3.14159265358979 * HIGHPASS_CUTOFF_HZ);
	double dt = 1.0 / sample_rate;
	apu->highpass_coefficient = (float)(rc / (rc + dt));
	apu->highpass_prev_in = 0.0f;
	apu->highpass_prev_out = 0.0f;
}

// ----- Envelope -----

static void clockEnvelope(struct NF_APU_Envelope* env) {
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

static uint8_t envelopeOutput(struct NF_APU_Envelope* env) {
	return env->constant ? env->volume : env->decay;
}

// ----- Pulse -----

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

static void clockSweep(struct NF_APU_Pulse* pulse) {
	if (pulse->sweep_divider == 0 && pulse->sweep_enabled && pulse->sweep_shift > 0 && !pulseMuted(pulse)) {
		pulse->timer_period = sweepTargetPeriod(pulse);
	}
	if (pulse->sweep_divider == 0 || pulse->sweep_reload) {
		pulse->sweep_divider = pulse->sweep_period;
		pulse->sweep_reload = false;
	}
	else { pulse->sweep_divider--; }
}

static void clockPulseTimer(struct NF_APU_Pulse* pulse) {
	if (pulse->timer == 0) {
		pulse->timer = pulse->timer_period;
		pulse->sequence_pos = (pulse->sequence_pos + 1) & 0x07;
	}
	else { pulse->timer--; }
}

static uint8_t pulseOutput(struct NF_APU_Pulse* pulse) {
	if (pulse->length == 0 || pulseMuted(pulse)) { return 0; }
	if (duty_table[pulse->duty][pulse->sequence_pos] == 0) { return 0; }
	return envelopeOutput(&pulse->envelope);
}

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

// ----- Triangle -----

static void clockLinearCounter(struct NF_APU_Triangle* tri) {
	if (tri->linear_reload_flag) { tri->linear_counter = tri->linear_reload; }
	else if (tri->linear_counter > 0) { tri->linear_counter--; }
	if (!tri->control) { tri->linear_reload_flag = false; }
}

static void clockTriangleTimer(struct NF_APU_Triangle* tri) {
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

// ----- Noise -----

static void clockNoiseTimer(struct NF_APU_Noise* noise) {
	if (noise->timer == 0) {
		noise->timer = noise->timer_period;
		uint16_t other_bit = noise->mode ? (noise->shift_register >> 6) : (noise->shift_register >> 1);
		uint16_t feedback = (noise->shift_register ^ other_bit) & 0x01;
		noise->shift_register = (noise->shift_register >> 1) | (feedback << 14);
	}
	else { noise->timer--; }
}

static uint8_t noiseOutput(struct NF_APU_Noise* noise) {
	if (noise->length == 0 || (noise->shift_register & 0x01)) { return 0; }
	return envelopeOutput(&noise->envelope);
}

// ----- Frame counter -----

// Quarter frame: envelopes and the triangle's linear counter
static void clockQuarterFrame(struct AudioProcessingUnit* apu) {
	clockEnvelope(&apu->pulse1.envelope);
	clockEnvelope(&apu->pulse2.envelope);
	clockEnvelope(&apu->noise.envelope);
	clockLinearCounter(&apu->triangle);
}

// Half frame: length counters and sweep units
static void clockHalfFrame(struct AudioProcessingUnit* apu) {
	if (apu->pulse1.length > 0 && !apu->pulse1.envelope.loop) { apu->pulse1.length--; }
	if (apu->pulse2.length > 0 && !apu->pulse2.envelope.loop) { apu->pulse2.length--; }
	if (apu->triangle.length > 0 && !apu->triangle.control) { apu->triangle.length--; }
	if (apu->noise.length > 0 && !apu->noise.envelope.loop) { apu->noise.length--; }
	clockSweep(&apu->pulse1);
	clockSweep(&apu->pulse2);
}

static void clockFrameCounter(struct AudioProcessingUnit* apu) {
	apu->frame_cycle++;

	switch (apu->frame_cycle) {
	case FRAME_STEP_1: clockQuarterFrame(apu); break;
	case FRAME_STEP_2: clockQuarterFrame(apu); clockHalfFrame(apu); break;
	case FRAME_STEP_3: clockQuarterFrame(apu); break;
	case FRAME_STEP_4:
		if (!apu->frame_mode_5step) {
			clockQuarterFrame(apu);
			clockHalfFrame(apu);
			if (!apu->irq_inhibit) { apu->frame_irq = true; }
			apu->frame_cycle = 0;
		}
		break;
	case FRAME_STEP_5:
		// Only reached in 5-step mode
		clockQuarterFrame(apu);
		clockHalfFrame(apu);
		apu->frame_cycle = 0;
		break;
	}
}

// ----- Mixer -----

// Non-linear mixing approximation from the NESdev wiki. Output is in the range 0.0 - ~1.0
static float mix(struct AudioProcessingUnit* apu) {
	uint8_t p1 = pulseOutput(&apu->pulse1);
	uint8_t p2 = pulseOutput(&apu->pulse2);
	uint8_t t = triangleOutput(&apu->triangle);
	uint8_t n = noiseOutput(&apu->noise);

	float pulse_out = 0.0f;
	if (p1 + p2 > 0) { pulse_out = 95.88f / (8128.0f / (float)(p1 + p2) + 100.0f); }

	float tnd_out = 0.0f;
	float tnd_sum = (float)t / 8227.0f + (float)n / 12241.0f;
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

// ----- Public interface -----

void NF_APU_tickClock(struct AudioProcessingUnit* apu) {
	clockFrameCounter(apu);

	// The triangle and noise timers run at the CPU rate, the pulse timers at half of it
	clockTriangleTimer(&apu->triangle);
	clockNoiseTimer(&apu->noise);
	if (apu->odd_cycle) {
		clockPulseTimer(&apu->pulse1);
		clockPulseTimer(&apu->pulse2);
	}
	apu->odd_cycle = !apu->odd_cycle;

	generateSample(apu);
}

void NF_APU_writeRegister(struct AudioProcessingUnit* apu, uint16_t address, uint8_t data) {
	switch (address) {

	// Pulse 1 and 2
	case 0x4000: case 0x4001: case 0x4002: case 0x4003:
		writePulse(&apu->pulse1, address & 0x03, data);
		break;
	case 0x4004: case 0x4005: case 0x4006: case 0x4007:
		writePulse(&apu->pulse2, address & 0x03, data);
		break;

	// Triangle
	case 0x4008:
		apu->triangle.control = (data & 0x80) != 0;
		apu->triangle.linear_reload = data & 0x7F;
		break;
	case 0x4009: break;  // Unused
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
	case 0x400D: break;  // Unused
	case 0x400E:
		apu->noise.mode = (data & 0x80) != 0;
		apu->noise.timer_period = noise_period_table[data & 0x0F];
		break;
	case 0x400F:
		if (apu->noise.enabled) { apu->noise.length = length_table[data >> 3]; }
		apu->noise.envelope.start = true;
		break;

	// DMC (not implemented yet)
	case 0x4010: case 0x4011: case 0x4012: case 0x4013: break;

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
		break;

	// Frame counter: resets the sequence, and 5-step mode clocks everything immediately
	case 0x4017:
		apu->frame_mode_5step = (data & 0x80) != 0;
		apu->irq_inhibit = (data & 0x40) != 0;
		if (apu->irq_inhibit) { apu->frame_irq = false; }
		apu->frame_cycle = 0;
		if (apu->frame_mode_5step) {
			clockQuarterFrame(apu);
			clockHalfFrame(apu);
		}
		break;
	}
}

uint8_t NF_APU_readStatus(struct AudioProcessingUnit* apu) {
	uint8_t status = 0;
	if (apu->pulse1.length > 0) { status |= 0x01; }
	if (apu->pulse2.length > 0) { status |= 0x02; }
	if (apu->triangle.length > 0) { status |= 0x04; }
	if (apu->noise.length > 0) { status |= 0x08; }
	if (apu->frame_irq) { status |= 0x40; }
	apu->frame_irq = false;
	return status;
}
