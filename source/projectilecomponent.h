#pragma once

#include "world.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct UplProjectile {
  float lifetime;
} UplProjectile;
extern ECS_COMPONENT_DECLARE(UplProjectile);

#ifdef __cplusplus
}
#endif
