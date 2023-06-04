/*
使用一个独立线程模拟硬件混音器
*/

#include "mixer.h"
#include "sound.h"
#include <SDL2/SDL.h>
#include <SDL_mixer.h>

#define MIXER_STATE_IDLE 0
#define MIXER_STATE_PLAYING 1
#define MIXER_STATE_PREPARED 2

static int g_mixer_state = MIXER_STATE_IDLE;
static char * g_MixerBuff = NULL;
static uint32_t g_MixerCount = 0;
static uint32_t g_Volume = 70;
Mix_Chunk wave;

Uint16 convertFormat(uint16_t format)
{
    switch (format)
    {
    case AFMT_U8:
        return AUDIO_U8;
    case AFMT_S16_LE:
        return AUDIO_S16LSB;
    default:
        return MIX_DEFAULT_FORMAT;
    }
}

//插值
char* convertInsert(float percent, char* buff, int size)
{
    int size_real = size * percent;
    char*newBuff = (char*)malloc(size_real);
    for (int i = 0; i < size_real/2; i++)
    {
        int index = (int)(i / percent);
        if (index >= size/2)
        {
            ((uint16_t*)newBuff)[i] = ((uint16_t*)buff)[size-1];
        }
        ((uint16_t*)newBuff)[i] = ((uint16_t*)buff)[index];
    }

    free(buff);

    return newBuff;
}

void* MixerThreadRun(void* data)
{
	waveout_args* args = (waveout_args*)data;

    g_Volume = args->volume;

    uint32_t ret = -1;

    int audio_rate = args->sample_rate; // MIX_DEFAULT_FREQUENCY;
    Uint16 audio_format = convertFormat(args->format);
    int audio_channels = MIX_DEFAULT_CHANNELS;

    /* Open the audio device */
    if (ret = Mix_OpenAudio(audio_rate, audio_format, audio_channels, 4096) < 0)
    {
        SDL_Log("Couldn't open audio: %s\n", SDL_GetError());
        exit(2);
    }

    Mix_QuerySpec(&audio_rate, &audio_format, &audio_channels);
    SDL_Log("Opened audio at %d Hz %d bit%s %s", audio_rate,
        (audio_format & 0xFF),
        (SDL_AUDIO_ISFLOAT(audio_format) ? " (float)" : ""),
        (audio_channels > 2) ? "surround" :
        (audio_channels > 1) ? "stereo" : "mono");

	while (1)
	{
        switch (g_mixer_state)
        {
        case MIXER_STATE_PREPARED:
        {
            //g_MixerBuff = convertInsert(audio_rate / args->sample_rate * 1.0f, g_MixerBuff, g_MixerCount);
            wave.abuf = (uint8_t*)g_MixerBuff;
            wave.alen = g_MixerCount;
            wave.volume = args->volume;
            wave.allocated = 0;
            if (g_Volume != wave.volume)
            {
                wave.volume = g_Volume;
            }

            /* Play and then exit */
            uint32_t ret = Mix_PlayChannel(-1, &wave, 0);

            g_mixer_state = MIXER_STATE_PLAYING;
        
            break;
        }
        case MIXER_STATE_PLAYING:
        {
            if (g_Volume != args->volume)
            {
                args->volume = g_Volume;
                Mix_Volume(-1, g_Volume);
            }

            if (Mix_Playing(-1) > 0)
            {
                SDL_Delay(1);
            }
            else
            {
                free(g_MixerBuff);
                g_MixerBuff = NULL;

                g_mixer_state = MIXER_STATE_IDLE;
            }
        
            break;
        }
        case MIXER_STATE_IDLE:
            SDL_Delay(1);
            break;
        default:
            SDL_Delay(1);
            break;
        }
	}
}

uint32_t MixerWriteBuff(char* buffer, int count)
{
    if (g_mixer_state == MIXER_STATE_IDLE)
    {
        g_MixerBuff = buffer;
        g_MixerCount = count;

        g_mixer_state = MIXER_STATE_PREPARED;
    }

    return 1;
}

uint32_t MixerPlaying()
{
    if (g_mixer_state != MIXER_STATE_IDLE)
    {
        return 0;
    }
    return 1;
}

void MixerSetVolume(uint32_t vol)
{
    g_Volume = vol;
}