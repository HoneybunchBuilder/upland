#include "projectilecomponent.h"

extern "C" {

ecs_entity_t upl_register_projectile_comp(TbWorld *world) {
  flecs::world ecs(world->ecs);
  ecs.component<UplProjectile>();
  return 0;
}

bool upl_load_projectile_comp(TbWorld *world, ecs_entity_t ent,
                              const char *source_path, const cgltf_node *node,
                              json_object *json) {
  (void)source_path;
  (void)node;
  (void)json;
  // Tag the entity as being a projectile
  flecs::world ecs(world->ecs);
  ecs.entity(ent).set<UplProjectile>({0});
  return true;
}

void upl_destroy_projectile_comp(TbWorld *world, ecs_entity_t ent) {
  (void)world;
  (void)ent;
}

TB_REGISTER_COMP(upl, projectile)
}