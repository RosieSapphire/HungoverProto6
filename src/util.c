#include "util.h"

void t3d_vec3_scale(T3DVec3 *dst, const T3DVec3 *src, const float s)
{
	dst->v[0] = src->v[0] * s;
	dst->v[1] = src->v[1] * s;
	dst->v[2] = src->v[2] * s;
}
