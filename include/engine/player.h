#ifndef _ENGINE_PLAYER_H_
#define _ENGINE_PLAYER_H_

#include <libdragon.h>
#include <t3d/t3dmath.h>

typedef struct
{
	float pos_old[3], pos_new[3];
	float angles_old[2], angles_new[2];
	u64 ticks;
} player_t;

void player_init(player_t *p, const float x, const float y, const float z,
		 const float yaw, const float pitch);
void player_get_camera(const player_t *p, T3DVec3 *eye, T3DVec3 *foc,
		       T3DVec3 *up, const float subtick);
void player_get_vecs(T3DVec3 *forw, T3DVec3 *side, const T3DVec3 *eye,
		     const T3DVec3 *foc);
void player_update(player_t *p, const joypad_inputs_t held);

#endif /* _ENGINE_PLAYER_H_ */
