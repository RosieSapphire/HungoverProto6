#include <libdragon.h>
#include <t3d/t3d.h>
#include <t3d/t3dmodel.h>

#include "config.h"
#include "types.h"

#include "engine/noise.h"
#include "engine/player.h"

#define THETA_SPEED 64

static player_t player;

static surface_t depth_buffer;
static rspq_block_t *room_dl;

static const u8 ambi_col[4] = {0x64, 0x64, 0x64, 0xFF};
static const u8 light_col[4] = {0xFF, 0xFF, 0xFF, 0xFF};
static T3DVec3 light_dir;
static T3DViewport viewport;
static T3DModel *room_t3dm;
static T3DMat4 global_scale_mat;
static T3DMat4 room_mat;
static T3DMat4FP *global_scale_mat_fp;
static T3DMat4FP *room_mat_fp;

static void init(void)
{
	srand(TICKS_READ());
	debug_init_isviewer();
	debug_init_usblog();

	rdpq_init();
	rdpq_debug_start();
	display_init(DSP_RES, DSP_DEP, DSP_BUF_CNT, DSP_GAM, DSP_FIL);
	dfs_init(DFS_DEFAULT_LOCATION);
	joypad_init();
	asset_init_compression(2);
	timer_init();
	depth_buffer = surface_alloc(FMT_RGBA16, DSP_WID, DSP_HEI);

	t3d_init((T3DInitParams) {});
	viewport = t3d_viewport_create();
	room_t3dm = t3d_model_load("rom:/room.t3dm");
	global_scale_mat_fp = malloc_uncached(sizeof(T3DMat4FP));
	room_mat_fp = malloc_uncached(sizeof(T3DMat4FP));
	t3d_viewport_set_projection(&viewport, PROJ_FOV, PROJ_NEAR, PROJ_FAR);
	t3d_mat4_identity(&global_scale_mat);
	t3d_mat4_scale(&global_scale_mat, 1, 1, 1);
	t3d_mat4_to_fixed(global_scale_mat_fp, &global_scale_mat);

	player_init(&player, 0, 0, 128, -90, 0);
}

static void render_dls_gen(void)
{
	if (!room_dl)
	{
		rspq_block_begin();
		t3d_matrix_push(room_mat_fp);
		t3d_model_draw(room_t3dm);
		t3d_matrix_pop(1);
		room_dl = rspq_block_end();
	}
}

static void render(const float subtick, const u64 timer_old,
		   const u64 timer_new)
{
	T3DVec3 eye, foc, up;

	player_get_camera(&player, &eye, &foc, &up, subtick);
	t3d_viewport_look_at(&viewport, &eye, &foc, &up);

	t3d_mat4_identity(&room_mat);
	t3d_mat4_to_fixed(room_mat_fp, &room_mat);

	rdpq_attach(display_get(), &depth_buffer);
	t3d_frame_start();
	t3d_viewport_attach(&viewport);
	t3d_screen_clear_color((color_t) {});
	t3d_screen_clear_depth();
	player_get_vecs(&light_dir, NULL, &eye, &foc);
	light_dir.v[0] *= -1;
	light_dir.v[1] *= -1;
	light_dir.v[2] *= -1;
	t3d_vec3_norm(&light_dir);
	t3d_light_set_ambient(ambi_col);
	t3d_light_set_directional(0, light_col, &light_dir);
	t3d_light_set_count(1);
	rdpq_mode_antialias(AA_STANDARD);
	rdpq_mode_dithering(DITHER_NOISE_NONE);

	render_dls_gen();
	t3d_matrix_push(global_scale_mat_fp);
	rspq_block_run(room_dl);
	t3d_matrix_pop(1);
	rdpq_detach_show();
}

int main(void)
{
	u64 timer_accum, timer_old, timer_new, timer_dif;
	float subtick;

	init();
	timer_accum = 0;
	timer_old = timer_ticks();
	for (;;)
	{
		timer_new = timer_ticks();
		timer_dif = timer_new - timer_old;
		timer_old = timer_new;
		timer_accum += timer_dif;
		while (timer_accum >= TICKS_PER_UPDATE)
		{
			joypad_poll();
			player_update(&player,
				      joypad_get_inputs(JOYPAD_PORT_1));
			timer_accum -= TICKS_PER_UPDATE;
		}
		subtick = (float)timer_accum / TICKS_PER_UPDATE;
		render(subtick, timer_old, timer_new);
	}
}
