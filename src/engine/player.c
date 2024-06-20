#include "config.h"
#include "types.h"
#include "util.h"

#include "engine/noise.h"
#include "engine/player.h"

#define PLAYER_HEI (128 * 1)
#define PLAYER_MOVE_SPEED (8 * 1)
#define PLAYER_TURN_SPEED 0.08f

void player_init(player_t *p, const float x, const float y, const float z,
		 const float yaw, const float pitch)
{
	p->pos_old[0] = x;
	p->pos_old[1] = y;
	p->pos_old[2] = z;
	p->pos_new[0] = x;
	p->pos_new[1] = y;
	p->pos_new[2] = z;
	p->angles_old[0] = yaw;
	p->angles_old[1] = pitch;
	p->angles_new[0] = yaw;
	p->angles_new[1] = pitch;
}

void player_get_camera(const player_t *p, T3DVec3 *eye, T3DVec3 *foc,
		       T3DVec3 *up, const float subtick)
{
	const float rad[2] = {
		T3D_DEG_TO_RAD(
			t3d_lerp(p->angles_old[0], p->angles_new[0], subtick)),
		T3D_DEG_TO_RAD(
			t3d_lerp(p->angles_old[1], p->angles_new[1], subtick)),
	};

	*up = (T3DVec3) {{0, 1, 0}};
	t3d_vec3_add(
		eye,
		&(T3DVec3) {{/* pos (lerped) */
			     t3d_lerp(p->pos_old[0], p->pos_new[0], subtick),
			     t3d_lerp(p->pos_old[1], p->pos_new[1], subtick),
			     t3d_lerp(p->pos_old[2], p->pos_new[2], subtick)}},
		&(T3DVec3) {{0, PLAYER_HEI, 0}});
	t3d_vec3_add(foc, eye,
		     &(T3DVec3) {{
			     /* dir */
			     cosf(rad[0]) * cosf(rad[1]),
			     sinf(rad[1]),
			     sinf(rad[0]) * cosf(rad[1]),
		     }});
}

void player_get_vecs(T3DVec3 *forw, T3DVec3 *side, const T3DVec3 *eye,
		     const T3DVec3 *foc)
{
	t3d_vec3_diff(forw, foc, eye);
	t3d_vec3_norm(forw);
	if (side)
		t3d_vec3_cross(side, forw, &(T3DVec3) {{0, 1, 0}});
}

void player_update(player_t *p, const joypad_inputs_t held)
{
	const s8 stick[2] = {
		held.stick_x * (ABS(held.stick_x) > STICK_DEADZONE),
		held.stick_y * (ABS(held.stick_y) > STICK_DEADZONE),
	};
	T3DVec3 eye, foc, forw, side, up, move = {{0}};
	const int c_move[2] = {
		held.btn.c_right - held.btn.c_left,
		held.btn.c_up - held.btn.c_down,
	};

	/* angle */
	p->angles_old[0] = p->angles_new[0];
	p->angles_old[1] = p->angles_new[1];
	p->angles_new[0] += stick[0] * PLAYER_TURN_SPEED;
	p->angles_new[1] -= stick[1] * PLAYER_TURN_SPEED;

	/* position */
	p->pos_old[0] = p->pos_new[0];
	p->pos_old[1] = p->pos_new[1];
	p->pos_old[2] = p->pos_new[2];
	player_get_camera(p, &eye, &foc, &up, 1.0f);
	player_get_vecs(&forw, &side, &eye, &foc);
	forw.v[1] = 0;
	side.v[1] = 0;
	t3d_vec3_scale(&side, &side, c_move[0]);
	t3d_vec3_scale(&forw, &forw, c_move[1]);
	t3d_vec3_add(&move, &side, &forw);
	t3d_vec3_norm(&move);
	t3d_vec3_scale(&move, &move, PLAYER_MOVE_SPEED);
	t3d_vec3_add((T3DVec3 *)p->pos_new, (T3DVec3 *)p->pos_new, &move);
}
