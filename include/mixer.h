#ifndef _MIXER_H_
#define _MIXER_H_

#include <stdint.h>

void* MixerThreadRun(void* data);
uint32_t MixerWriteBuff(char* buffer, int count);
uint32_t MixerPlaying();
void MixerSetVolume(uint32_t vol);


#endif