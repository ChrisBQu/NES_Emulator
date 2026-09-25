#ifndef NF_H_APU
#define NF_H_APU
#include "NF_Bus.h"
#include <stdint.h>
#include <stdbool.h>

// The APU (Audio Processing Unit) is built into the NES's CPU chip. It is mapped to these addresses:
//
// $4000-4003: Pulse 1   (duty/envelope, sweep, timer low, length counter load/timer high)
// $4004-4007: Pulse 2   (same layout as pulse 1)
// $4008-400B: Triangle  (linear counter, unused, timer low, length counter load/timer high)
// $400C-400F: Noise     (envelope, unused, mode/period, length counter load)
// $4010-4013: DMC       (not implemented yet)
// $4015:      Status    (write: enable channels, read: which channels are still playing, frame IRQ flag)
// $4017:      Frame counter (write only: 4-step/5-step mode, IRQ inhibit)

#define NF_APU_CPU_CLOCK_RATE 1789773.0

// Volume envelope, shared by the pulse and noise channels
struct NF_APU_Envelope {
	bool start;           // Set when the channel's 4th register is written; restarts the envelope
	bool loop;            // Loop the decay (this bit is also the length counter halt flag)
	bool constant;        // Use a constant volume instead of the decaying one
	uint8_t volume;       // Constant volume, or the divider period for the decay
	uint8_t divider;
	uint8_t decay;        // Decaying volume level, 15 -> 0
};

struct NF_APU_Pulse {
	bool is_pulse1;       // Pulse 1's sweep negates slightly differently (ones' complement) from pulse 2's
	bool enabled;
	uint8_t duty;         // Which of the 4 duty cycle sequences to use
	uint8_t sequence_pos; // Position in the 8-step duty sequence
	uint16_t timer_period;
	uint16_t timer;
	uint8_t length;       // Length counter: the channel is silenced when it reaches 0
	struct NF_APU_Envelope envelope;

	bool sweep_enabled;
	bool sweep_negate;
	bool sweep_reload;
	uint8_t sweep_period;
	uint8_t sweep_shift;
	uint8_t sweep_divider;
};

struct NF_APU_Triangle {
	bool enabled;
	bool control;              // Halts the length counter, and stops the linear counter reload flag from clearing
	bool linear_reload_flag;
	uint8_t linear_reload;
	uint8_t linear_counter;
	uint8_t length;
	uint8_t sequence_pos;      // Position in the 32-step triangle sequence
	uint16_t timer_period;
	uint16_t timer;
};

struct NF_APU_Noise {
	bool enabled;
	bool mode;                 // Short mode: feedback from bit 6 instead of bit 1, giving a metallic tone
	uint16_t shift_register;   // 15-bit linear feedback shift register
	uint16_t timer_period;
	uint16_t timer;
	uint8_t length;
	struct NF_APU_Envelope envelope;
};

struct AudioProcessingUnit {
	struct NES_Console* bus;

	struct NF_APU_Pulse pulse1;
	struct NF_APU_Pulse pulse2;
	struct NF_APU_Triangle triangle;
	struct NF_APU_Noise noise;

	// Frame counter: clocks the envelopes, length counters, sweeps and linear counter at ~240Hz
	bool frame_mode_5step;
	bool irq_inhibit;
	bool frame_irq;            // Set by the 4-step sequence; nothing raises a CPU IRQ from it yet
	uint32_t frame_cycle;
	bool odd_cycle;            // Pulse timers are clocked every other CPU cycle

	// Downsampling from the CPU clock rate to the output sample rate
	double sample_rate;
	double sample_clock;
	float sample_sum;
	uint32_t sample_count;

	// High-pass filter to remove the DC offset (the mixer output is always positive)
	float highpass_coefficient;
	float highpass_prev_in;
	float highpass_prev_out;
};

// Must be called once to create the APU object
struct AudioProcessingUnit* NF_initAPU();

// Set the rate of the samples sent to the console's audioOutFunc (e.g. 44100)
void NF_APU_setSampleRate(struct AudioProcessingUnit* apu, double sample_rate);

// Advance the APU by one CPU cycle
void NF_APU_tickClock(struct AudioProcessingUnit* apu);

// Write to one of the APU registers ($4000-$4013, $4015, $4017)
void NF_APU_writeRegister(struct AudioProcessingUnit* apu, uint16_t address, uint8_t data);

// Read the status register ($4015). This clears the frame IRQ flag
uint8_t NF_APU_readStatus(struct AudioProcessingUnit* apu);

#endif