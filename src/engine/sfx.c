#include "config.h"

#include "engine/sfx.h"

wav64_t sfx_45acp_near, sfx_45acp_far;

void sfx_init(void)
{
	audio_init(AUDIO_FREQ, AUDIO_BUF_CNT);
	mixer_init(MIXER_CH_CNT);
	wav64_open(&sfx_45acp_near, "rom:/45acp_near.wav64");
	wav64_open(&sfx_45acp_far, "rom:/45acp_far.wav64");
	mixer_set_vol(0.5f);
}

void sfx_gunshot_play(const int which)
{
	wav64_play((wav64_t *[2]){ &sfx_45acp_near, &sfx_45acp_far }[which],
		   which);
}
