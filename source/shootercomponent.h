#pragma once

#include "world.h"

typedef uint64_t ecs_entity_t;

typedef struct UplShooter {
  float fire_rate;
  float last_fire_time;
  const char *prefab_name;
  ecs_entity_t projectile_prefab;
} UplShooter;
