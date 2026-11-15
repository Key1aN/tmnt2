#pragma once
/* MinGW build only: libstdc++ does not declare these C99 float overloads in namespace std. */
#include <math.h>
namespace std { using ::logf; using ::expf; using ::powf; using ::sqrtf; using ::sinf; using ::cosf; using ::fabsf; using ::floorf; using ::ceilf; using ::atan2f; using ::fmodf; }
