#include "sound.h"
#include <pthread.h>
#include "mixer.h"
#include <assert.h>

uint32_t waveout_open(waveout_args* args)
{
	printf("args channel %d format %d sample_rate %d volume %d, channel %d\n",
		args->channel, args->format, args->sample_rate, args->volume, args->channel);

    pthread_t tid;
    int ret = pthread_create(&tid,
        NULL,
        MixerThreadRun,
        (void *)args);
    if (ret)
    {
        printf("pthread_create dingooRun failed\n");
        assert(0);
    }

	return 1;
}

uint32_t waveout_write(uint32_t inst, char* buffer, int count)
{
    return MixerWriteBuff(buffer, count);
}

uint32_t waveout_can_write()
{
    return MixerPlaying();
}

uint32_t waveout_set_volume(uint32_t vol)
{
    MixerSetVolume(vol);
    return 1;
}
