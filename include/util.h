#ifndef _UTIL_H_
#define _UTIL_H_

#include <t3d/t3dmath.h>

#define ABS(X) ((X < 0) ? -X : X)

void t3d_vec3_scale(T3DVec3 *dst, const T3DVec3 *x, const float s);

#endif /* _UTIL_H_ */
