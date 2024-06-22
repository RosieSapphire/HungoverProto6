#include <libdragon.h>
#include <t3d/t3d.h>
#include <t3d/t3dmodel.h>
#include <t3d/t3danim.h>

#include "util.h"
#include "config.h"
#include "types.h"

#include "engine/noise.h"
#include "engine/player.h"
#include "engine/sfx.h"

#define THETA_SPEED 64
#define WEAPON_HELD_POS_LERP_SPEED 12
#define WEAPON_HELD_ANG_LERP_SPEED 18

static player_t player;

static surface_t depth_buffer;
static rspq_block_t *testroom_dl;

/* main */
static const u8 ambi_col[4] = { 0x64, 0x64, 0x64, 0xFF };
static const u8 light_col[4] = { 0xFF, 0xFF, 0xFF, 0xFF };
static T3DVec3 light_dir;
static T3DViewport viewport;
static T3DMat4 viewmodel_mat;
static T3DMat4FP *viewmodel_mat_fp;
static T3DModel *testroom_t3dm;
static T3DMat4 testroom_mat;
static T3DMat4FP *testroom_mat_fp;

/* weapons */
static T3DModel *pistol_t3dm;
static T3DSkeleton pistol_skel;
static T3DAnim pistol_anim_idle, pistol_anim_fire;
static rspq_block_t *pistol_dl;
static T3DVec3 weapon_hold_target;
static T3DVec3 weapon_hold_current;
static float weapon_angle_target[2];
static float weapon_angle_current[2];

/* visual */
static float camshake_val;
static float camshake_dir[2];

static void init(void)
{
	srand(TICKS_READ());
	debug_init_isviewer();
	debug_init_usblog();

	rdpq_init();
	rdpq_debug_start();
	dfs_init(DFS_DEFAULT_LOCATION);
	sfx_init();
	display_init(DSP_RES, DSP_DEP, DSP_BUF_CNT, DSP_GAM, DSP_FIL);
	joypad_init();
	asset_init_compression(2);
	timer_init();
	depth_buffer = surface_alloc(FMT_RGBA16, DSP_WID, DSP_HEI);

	t3d_init((T3DInitParams){});
	viewport = t3d_viewport_create();
	testroom_t3dm = t3d_model_load("rom:/testroom.t3dm");
	testroom_mat_fp = malloc_uncached(sizeof(T3DMat4FP));
	viewmodel_mat_fp = malloc_uncached(sizeof(T3DMat4FP));
	t3d_viewport_set_projection(&viewport, PROJ_FOV, PROJ_NEAR, PROJ_FAR);

	pistol_t3dm = t3d_model_load("rom:/pistol.t3dm");
	pistol_skel = t3d_skeleton_create(pistol_t3dm);
	pistol_anim_idle = t3d_anim_create(pistol_t3dm, "Idle");
	t3d_anim_set_looping(&pistol_anim_idle, 1);
	t3d_anim_set_playing(&pistol_anim_idle, 1);
	t3d_anim_set_time(&pistol_anim_idle, 0.0f);
	t3d_anim_attach(&pistol_anim_idle, &pistol_skel);
	pistol_anim_fire = t3d_anim_create(pistol_t3dm, "Fire");
	t3d_anim_set_looping(&pistol_anim_fire, 0);
	t3d_anim_set_playing(&pistol_anim_fire, 0);
	t3d_anim_set_time(&pistol_anim_fire, 0.0f);
	t3d_anim_attach(&pistol_anim_fire, &pistol_skel);
	player_init(&player, 0, 0, 128, -90, 0);
}

static void update(void)
{
	joypad_buttons_t press = joypad_get_buttons_pressed(JOYPAD_PORT_1);
	joypad_poll();
	camshake_val *= 0.84f;
	if (camshake_val < 0.01f)
		camshake_val = 0;
	if (press.z) {
		camshake_val += (float)(0xBFFE + (rand() & 0x3FFF)) / 0x3FFF;
		if (camshake_val > 100)
			camshake_val = 100;
		for (int i = 0; i < 2; i++)
			camshake_dir[i] = ((u16)rand() - (UINT16_MAX >> 1));
		const float camshake_len =
			sqrtf(camshake_dir[0] * camshake_dir[0] +
			      camshake_dir[1] * camshake_dir[1]);
		if (camshake_len) {
			camshake_dir[0] /= camshake_len;
			camshake_dir[1] /= camshake_len;
			if (camshake_dir[1] < 0)
				camshake_dir[1] *= -1;
		}
		camshake_dir[0] *= 0.2f;
		t3d_anim_set_playing(&pistol_anim_fire, 1);
		t3d_anim_set_time(&pistol_anim_idle, 0.0f);
		t3d_anim_set_time(&pistol_anim_fire, 0.0f);
		sfx_gunshot_play(MIXER_CH_GUNSHOT_NEAR);
	}
	t3d_anim_update(&pistol_anim_idle,
			SECONDS_PER_UPDATE * (-pistol_anim_fire.isPlaying + 1));
	t3d_anim_update(&pistol_anim_fire, SECONDS_PER_UPDATE);
	t3d_skeleton_update(&pistol_skel);
	player_update(&player, joypad_get_inputs(JOYPAD_PORT_1), press,
		      camshake_val, camshake_dir);
	T3DVec3 eye, foc, up, forw, side;
	player_get_camera(&player, &eye, &foc, &up, 1.0f, camshake_val,
			  camshake_dir);
	player_get_vecs(&forw, &side, &eye, &foc);
	weapon_hold_target = (T3DVec3){ {
		player.pos_new[0],
		player.pos_new[1] + PLAYER_ARM_HEI,
		player.pos_new[2],
	} };
	weapon_angle_target[0] = player.angles_new[0];
	weapon_angle_target[1] = player.angles_new[1];
	t3d_vec3_scale(&forw, &forw, 17);
	t3d_vec3_scale(&side, &side, 7);
	t3d_vec3_add(&weapon_hold_target, &weapon_hold_target, &forw);
	t3d_vec3_add(&weapon_hold_target, &weapon_hold_target, &side);
	t3d_vec3_lerp(&weapon_hold_current, &weapon_hold_current,
		      &weapon_hold_target,
		      SECONDS_PER_UPDATE * WEAPON_HELD_POS_LERP_SPEED);
	weapon_angle_current[0] =
		t3d_lerp(weapon_angle_target[0], weapon_angle_target[0],
			 SECONDS_PER_UPDATE * WEAPON_HELD_ANG_LERP_SPEED);
	weapon_angle_current[1] =
		t3d_lerp(weapon_angle_target[1], weapon_angle_target[1],
			 SECONDS_PER_UPDATE * WEAPON_HELD_ANG_LERP_SPEED);
}

static void render(const float subtick, const u64 timer_old,
		   const u64 timer_new)
{
	T3DVec3 eye, foc, up;

	player_get_camera(&player, &eye, &foc, &up, subtick, camshake_val,
			  camshake_dir);

	t3d_mat4_identity(&testroom_mat);
	t3d_mat4_to_fixed(testroom_mat_fp, &testroom_mat);

	rdpq_attach(display_get(), &depth_buffer);
	t3d_frame_start();

	/* final buffer */
	t3d_screen_clear_color((color_t){});
	t3d_screen_clear_depth();
	t3d_viewport_attach(&viewport);
	player_get_vecs(&light_dir, NULL, &eye, &foc);
	light_dir.v[0] *= -1;
	light_dir.v[1] *= -1;
	light_dir.v[2] *= -1;
	t3d_vec3_norm(&light_dir);
	t3d_light_set_ambient(ambi_col);
	t3d_light_set_directional(0, light_col, &light_dir);
	t3d_light_set_count(1);
	t3d_viewport_look_at(&viewport, &eye, &foc, &up);
	rdpq_set_mode_standard();
	rdpq_mode_zbuf(1, 1);
	rdpq_mode_persp(1);
	rdpq_mode_antialias(AA_STANDARD);
	rdpq_mode_dithering(DITHER_NOISE_NONE);
	if (!testroom_dl) {
		rspq_block_begin();
		t3d_matrix_push(testroom_mat_fp);
		t3d_model_draw(testroom_t3dm);
		t3d_matrix_pop(1);
		testroom_dl = rspq_block_end();
	}
	rspq_block_run(testroom_dl);

	/* player viewmodel */
	rdpq_sync_pipe();
	rdpq_set_mode_standard();
	rdpq_mode_zbuf(1, 1);
	rdpq_mode_antialias(AA_STANDARD);
	rdpq_mode_dithering(DITHER_NOISE_NONE);
	t3d_mat4_identity(&viewmodel_mat);
	t3d_mat4_rotate(&viewmodel_mat, &(T3DVec3){ { 0, 1, 0 } },
			/********************
			 * TODO: LERP ANGLE *
			 ********************/
			player.angles_new[0]);
	t3d_mat4_translate(&viewmodel_mat, weapon_hold_current.v[0],
			   weapon_hold_current.v[1], weapon_hold_current.v[2]);
	t3d_mat4_to_fixed(viewmodel_mat_fp, &viewmodel_mat);
	if (!pistol_dl) {
		rspq_block_begin();
		t3d_matrix_push(viewmodel_mat_fp);
		t3d_model_draw_skinned(pistol_t3dm, &pistol_skel);
		t3d_matrix_pop(1);
		pistol_dl = rspq_block_end();
	}
	rspq_block_run(pistol_dl);
	rdpq_detach_show();
}

int main(void)
{
	u64 timer_accum, timer_old, timer_new, timer_dif;
	float subtick;

	init();
	timer_accum = 0;
	timer_old = timer_ticks();
	for (;;) {
		timer_new = timer_ticks();
		timer_dif = timer_new - timer_old;
		timer_old = timer_new;
		timer_accum += timer_dif;
		while (timer_accum >= TICKS_PER_UPDATE) {
			update();
			timer_accum -= TICKS_PER_UPDATE;
		}
		subtick = (float)timer_accum / TICKS_PER_UPDATE;
		render(subtick, timer_old, timer_new);

		if (!audio_can_write())
			continue;

		short *audiobuf = audio_write_begin();
		mixer_poll(audiobuf, audio_get_buffer_length());
		audio_write_end();
	}
}
