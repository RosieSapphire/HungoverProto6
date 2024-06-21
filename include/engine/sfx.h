#ifndef _ENGINE_SFX_H_
#define _ENGINE_SFX_H_

#include <libdragon.h>

enum {
	MIXER_CH_GUNSHOT_NEAR = 0,
	MIXER_CH_GUNSHOT_FAR,
	MIXER_CH_CNT,
};

extern wav64_t sfx_45acp_near, sfx_45acp_far;

void sfx_init(void);
void sfx_gunshot_play(const int which);

#endif /* _ENGINE_SFX_H_ */
