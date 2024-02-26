#include "shootercomponent.h"

#include <json.h>

ECS_COMPONENT_DECLARE(UplShooterDesc);
ECS_COMPONENT_DECLARE(UplShooter);

extern "C" {

typedef struct UplShooterDesc {
  float fire_rate;
  const char *prefab_name;
} UplShooterDesc;

void upl_shooter_on_set(flecs::entity ent, UplShooter &shooter) {
  if (!ent.has<UplShooter>()) {
    return;
  }
  flecs::world ecs = ent.world();
  shooter.projectile_prefab = ecs.lookup(shooter.prefab_name).id();
}

ecs_entity_t upl_register_shooter_comp(TbWorld *world) {
  flecs::world ecs(world->ecs);

  ecs.component<UplShooterDesc>()
      .member<float>("fire_rate")
      .member<const char *>("prefab_name");

  ecs.component<UplShooter>().on_set(upl_shooter_on_set);

  return ecs.component<UplShooterDesc>().id();
}

bool upl_load_shooter_comp(TbWorld *world, ecs_entity_t ent,
                           const char *source_path, const cgltf_node *node,
                           json_object *json) {
  (void)source_path;
  (void)node;
  flecs::world ecs(world->ecs);
  flecs::entity entity = ecs.entity(ent);

  UplShooter comp = {};

  json_object_object_foreach(json, key, value) {
    if (SDL_strcmp(key, "prefab_name") == 0) {
      comp.prefab_name = json_object_get_string(value);
    } else if (SDL_strcmp(key, "fire_rate") == 0) {
      comp.fire_rate = (float)json_object_get_double(value);
    }
  }

  entity.set<UplShooter>(comp);

  return true;
}

void upl_destroy_shooter_comp(TbWorld *world, ecs_entity_t ent) {
  (void)world;
  (void)ent;
}

TB_REGISTER_COMP(upl, shooter);
}
