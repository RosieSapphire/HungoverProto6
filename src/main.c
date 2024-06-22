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

static player_t player;

static surface_t depth_buffer;
static rspq_block_t *room_dl;

/* main */
static const u8 ambi_col[4] = { 0x64, 0x64, 0x64, 0xFF };
static const u8 light_col[4] = { 0xFF, 0xFF, 0xFF, 0xFF };
static T3DVec3 light_dir;
static T3DViewport viewport_main, viewport_viewmodel;
static T3DModel *room_t3dm;
static T3DMat4 room_mat;
static T3DMat4FP *room_mat_fp;

/* weapons */
static T3DModel *pistol_t3dm;
static T3DSkeleton pistol_skel;
static T3DAnim pistol_anim_idle, pistol_anim_fire;
static rspq_block_t *pistol_dl;
static T3DMat4 weapon_mat;
static T3DMat4FP *weapon_mat_fp;

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
	viewport_main = t3d_viewport_create();
	viewport_viewmodel = t3d_viewport_create();
	room_t3dm = t3d_model_load("rom:/room.t3dm");
	room_mat_fp = malloc_uncached(sizeof(T3DMat4FP));
	t3d_viewport_set_projection(&viewport_main, PROJ_FOV, PROJ_NEAR,
				    PROJ_FAR);
	t3d_viewport_set_projection(&viewport_viewmodel, PROJ_FOV, PROJ_NEAR,
				    PROJ_FAR);

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
	weapon_mat_fp = malloc_uncached(sizeof(*weapon_mat_fp));
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
		debugf("%f, %f\n", camshake_dir[0], camshake_dir[1]);
		t3d_anim_set_playing(&pistol_anim_fire, 1);
		t3d_anim_set_time(&pistol_anim_idle, 0.0f);
		t3d_anim_set_time(&pistol_anim_fire, 0.0f);
		sfx_gunshot_play(MIXER_CH_GUNSHOT_NEAR);
	}
	t3d_anim_update(&pistol_anim_idle,
			SECONDS_PER_UPDATE * (!pistol_anim_fire.isPlaying));
	t3d_anim_update(&pistol_anim_fire, SECONDS_PER_UPDATE);
	t3d_skeleton_update(&pistol_skel);
	player_update(&player, joypad_get_inputs(JOYPAD_PORT_1), press,
		      camshake_val, camshake_dir);
}

static void render(const float subtick, const u64 timer_old,
		   const u64 timer_new)
{
	T3DVec3 eye, foc, up;

	player_get_camera(&player, &eye, &foc, &up, subtick, camshake_val,
			  camshake_dir);

	t3d_mat4_identity(&room_mat);
	t3d_mat4_to_fixed(room_mat_fp, &room_mat);

	rdpq_attach(display_get(), &depth_buffer);
	t3d_frame_start();

	/* final buffer */
	rdpq_mode_zbuf(1, 1);
	t3d_screen_clear_color((color_t){});
	t3d_screen_clear_depth();
	t3d_viewport_attach(&viewport_main);
	player_get_vecs(&light_dir, NULL, &eye, &foc);
	light_dir.v[0] *= -1;
	light_dir.v[1] *= -1;
	light_dir.v[2] *= -1;
	t3d_vec3_norm(&light_dir);
	t3d_light_set_ambient(ambi_col);
	t3d_light_set_directional(0, light_col, &light_dir);
	t3d_light_set_count(1);
	t3d_viewport_look_at(&viewport_main, &eye, &foc, &up);
	rdpq_set_mode_standard();
	rdpq_mode_persp(1);
	rdpq_mode_antialias(AA_STANDARD);
	rdpq_mode_dithering(DITHER_NOISE_NONE);
	rdpq_mode_zbuf(1, 1);
	if (!room_dl) {
		rspq_block_begin();
		t3d_matrix_push(room_mat_fp);
		t3d_model_draw(room_t3dm);
		t3d_matrix_pop(1);
		room_dl = rspq_block_end();
	}
	rspq_block_run(room_dl);

	/* player viewmodel */
	t3d_mat4_identity(&weapon_mat);
	t3d_mat4_rotate(&weapon_mat, &(T3DVec3){ { 1, 0, 0 } },
			T3D_DEG_TO_RAD(((camshake_dir[1] * camshake_val * 8))));
	t3d_mat4_rotate(
		&weapon_mat, &(T3DVec3){ { 0, 1, 0 } },
		T3D_DEG_TO_RAD((-90 + (camshake_dir[0] * camshake_val * 4))));
	t3d_mat4_translate(&weapon_mat, 32, -32, 0);
	t3d_mat4_to_fixed(weapon_mat_fp, &weapon_mat);

	rdpq_sync_pipe();
	t3d_viewport_attach(&viewport_viewmodel);
	rdpq_set_mode_standard();
	rdpq_mode_zbuf(0, 0);
	rdpq_mode_antialias(AA_STANDARD);
	rdpq_mode_dithering(DITHER_NOISE_NONE);
	t3d_viewport_look_at(&viewport_viewmodel, &(T3DVec3){ { 0, 0, 64 } },
			     &(T3DVec3){ { 0, 0, 0 } }, &up);
	if (!pistol_dl) {
		rspq_block_begin();
		t3d_matrix_push(weapon_mat_fp);
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
