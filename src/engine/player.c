#include <t3d/t3dmodel.h>

#include "config.h"
#include "types.h"
#include "util.h"

#include "engine/sfx.h"
#include "engine/noise.h"
#include "engine/player.h"

#define PLAYER_HEI 128
#define PLAYER_ACCEL 0.16f
#define PLAYER_STOPSPEED (PLAYER_ACCEL * 10.0f)
#define PLAYER_MAXSPEED 320
#define PLAYER_FRICTION 4
#define PLAYER_FRONTSPEED 200
#define PLAYER_SIDESPEED 350
// #define PLAYER_GRAVITY 800
#define PLAYER_TURN_SPEED 0.08f

void player_weapon_dls_init(void)
{
}

void player_init(player_t *p, const float x, const float y, const float z,
		 const float yaw, const float pitch)
{
	p->pos_old[0] = x;
	p->pos_old[1] = y;
	p->pos_old[2] = z;
	p->pos_new[0] = x;
	p->pos_new[1] = y;
	p->pos_new[2] = z;
	p->vel[0] = 0;
	p->vel[1] = 0;
	p->vel[2] = 0;
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

	*up = (T3DVec3){ { 0, 1, 0 } };
	t3d_vec3_add(
		eye,
		&(T3DVec3){
			{ /* pos (lerped) */
			  t3d_lerp(p->pos_old[0], p->pos_new[0], subtick),
			  t3d_lerp(p->pos_old[1], p->pos_new[1], subtick),
			  t3d_lerp(p->pos_old[2], p->pos_new[2], subtick) } },
		&(T3DVec3){ { 0, PLAYER_HEI, 0 } });
	t3d_vec3_add(foc, eye,
		     &(T3DVec3){ {
			     /* dir */
			     cosf(rad[0]) * cosf(rad[1]),
			     sinf(rad[1]),
			     sinf(rad[0]) * cosf(rad[1]),
		     } });
}

void player_get_vecs(T3DVec3 *forw, T3DVec3 *side, const T3DVec3 *eye,
		     const T3DVec3 *foc)
{
	t3d_vec3_diff(forw, foc, eye);
	t3d_vec3_norm(forw);
	if (side)
		t3d_vec3_cross(side, forw, &(T3DVec3){ { 0, 1, 0 } });
}

static void player_update_look(player_t *p, const joypad_inputs_t held)
{
	const s8 stick[2] = {
		held.stick_x * (ABS(held.stick_x) > STICK_DEADZONE),
		held.stick_y * (ABS(held.stick_y) > STICK_DEADZONE),
	};

	p->angles_old[0] = p->angles_new[0];
	p->angles_old[1] = p->angles_new[1];
	p->angles_new[0] += stick[0] * PLAYER_TURN_SPEED;
	p->angles_new[1] -= stick[1] * PLAYER_TURN_SPEED;
}

static void player_update_friction(player_t *p)
{
	const float speed =
		sqrtf(p->vel[0] * p->vel[0] + p->vel[1] * p->vel[1] +
		      p->vel[2] * p->vel[2]);
	if (speed < 1) {
		p->vel[0] = 0.0f;
		p->vel[2] = 0.0f;
		return;
	}
	const float drop =
		(speed < PLAYER_STOPSPEED ? PLAYER_STOPSPEED : speed) *
		PLAYER_FRICTION * SECONDS_PER_UPDATE;
	float newspeed = speed - drop;
	if (newspeed < 0.0f)
		newspeed = 0.0f;
	newspeed /= speed;

	p->vel[0] *= newspeed;
	p->vel[1] *= newspeed;
	p->vel[2] *= newspeed;
}

static void player_update_acceleration(player_t *p, const T3DVec3 *wishdir,
				       const float wishspeed)
{
	const float currentspeed = t3d_vec3_dot((T3DVec3 *)p->vel, wishdir);
	const float addspeed = wishspeed - currentspeed;
	if (addspeed <= 0.0f)
		return;
	float accelspeed = wishspeed * PLAYER_ACCEL * SECONDS_PER_UPDATE;
	if (accelspeed > addspeed)
		accelspeed = addspeed;

	for (int i = 0; i < 3; i++)
		p->vel[i] += accelspeed * wishdir->v[i];
}

static void player_update_air_movement(player_t *p, const joypad_inputs_t held)
{
	T3DVec3 forw, side, eye, foc, up;
	player_get_camera(p, &eye, &foc, &up, 1.0f);
	player_get_vecs(&forw, &side, &eye, &foc);
	forw.v[1] = 0.0f;
	side.v[1] = 0.0f;
	t3d_vec3_norm(&forw);
	t3d_vec3_norm(&side);
	const float fmove =
		(held.btn.c_up - held.btn.c_down) * PLAYER_FRONTSPEED;
	const float smove =
		(held.btn.c_right - held.btn.c_left) * PLAYER_SIDESPEED;
	T3DVec3 wishvel = (T3DVec3){ {
		forw.v[0] * fmove + side.v[0] * smove,
		0,
		forw.v[2] * fmove + side.v[2] * smove,
	} };
	float wishspeed = t3d_vec3_len(&wishvel);
	T3DVec3 wishdir;
	if (wishspeed)
		t3d_vec3_scale(&wishdir, &wishvel, 1.0f / wishspeed);
	else
		wishdir = (T3DVec3){ { 0, 0, 0 } };
	if (wishspeed > PLAYER_MAXSPEED) {
		t3d_vec3_scale(&wishvel, &wishvel, PLAYER_MAXSPEED / wishspeed);
		wishspeed = PLAYER_MAXSPEED;
	}

	p->vel[1] = 0;
	player_update_acceleration(p, &wishdir, wishspeed);
	// p->vel[1] -= PLAYER_GRAVITY * SECONDS_PER_UPDATE;
	for (int i = 0; i < 3; i++)
		p->pos_old[i] = p->pos_new[i];
	t3d_vec3_add((T3DVec3 *)p->pos_new, (T3DVec3 *)p->pos_new,
		     (T3DVec3 *)p->vel);
	/* TODO: not on ground, so little effect on velocity
	PM_AirAccelerate(wishdir, wishspeed, movevars.accelerate);

	// add gravity
	pmove.velocity[2] -=
		movevars.entgravity * movevars.gravity * frametime;

	PM_FlyMove();
	*/
}

void player_update(player_t *p, const joypad_inputs_t held,
		   const joypad_buttons_t press)
{
	player_update_look(p, held);
	/* TODO: Implement Jumping */
	player_update_friction(p);
	player_update_air_movement(p, held);

	if (press.z)
		sfx_gunshot_play(MIXER_CH_GUNSHOT_NEAR);
	if (press.r)
		sfx_gunshot_play(MIXER_CH_GUNSHOT_FAR);
}
