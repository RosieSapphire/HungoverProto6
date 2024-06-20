#include "util.h"

void t3d_vec3_scale(T3DVec3 *dst, const T3DVec3 *x, const float s)
{
	dst->v[0] = x->v[0] * s;
	dst->v[1] = x->v[1] * s;
	dst->v[2] = x->v[2] * s;
}
