#ifndef _SOUND_H_
#define _SOUND_H_

#include <stdint.h>
#include <stdbool.h>
#include "app.h"
#include <unicorn/unicorn.h>


/* Audio Sample Format */
#define	AFMT_U8			8
#define AFMT_S16_LE		16

// Waveout types.
typedef struct {
	uint32_t sample_rate;
	uint16_t format;
	uint8_t  channel;
	uint8_t  volume;
} waveout_args;

typedef void waveout_inst;

// The following come from joyrider and from disassembly.
extern uint32_t waveout_open(waveout_args* args);
extern uint32_t waveout_write(uint32_t inst, char* buffer, int count);
extern uint32_t waveout_close(uint32_t inst);
extern uint32_t waveout_can_write();
extern uint32_t waveout_set_volume(uint32_t vol);

#endif