#pragma once

#include "simd.h"

typedef uint64_t ecs_entity_t;

typedef struct UplProjectile {
  float lifetime;
} UplProjectile;

typedef struct UplShooter {
  const char *prefab_name;
  float fire_rate;
  ecs_entity_t projectile_prefab;
  float last_fire_time;
} UplShooter;

typedef struct UplWarp {
  const char *target_name;
} UplWarp;
