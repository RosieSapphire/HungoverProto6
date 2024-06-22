#ifndef _CONFIG_H_
#define _CONFIG_H_

#define DSP_WID 320
#define DSP_HEI 240
#define DSP_RES RESOLUTION_320x240
#define DSP_DEP DEPTH_16_BPP
#define DSP_BUF_CNT 2
#define DSP_GAM GAMMA_NONE
#define DSP_FIL FILTERS_RESAMPLE_ANTIALIAS

#define UPDATES_PER_SECOND 60
#define SECONDS_PER_UPDATE (1.0f / UPDATES_PER_SECOND)
#define TICKS_PER_UPDATE (SECONDS_PER_UPDATE * TICKS_PER_SECOND)

#define PROJ_FOV T3D_DEG_TO_RAD(90)
#define PROJ_NEAR 8
#define PROJ_FAR 512

#define STICK_DEADZONE 2

#define AUDIO_FREQ 44100
#define AUDIO_BUF_CNT 1

#endif /* _CONFIG_H_ */
