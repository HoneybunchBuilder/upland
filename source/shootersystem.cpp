#include "shootersystem.h"

#include "assetsystem.h"
#include "inputsystem.h"
#include "meshcomponent.h"
#include "physicssystem.hpp"
#include "profiling.h"
#include "rigidbodycomponent.h"
#include "tbcommon.h"
#include "transformcomponent.h"
#include "world.h"

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/PhysicsSystem.h>

#include "physlayers.h"

#include <flecs.h>
#include <json.h>

void warp_trigger(ecs_world_t *world, ecs_entity_t user_e,
                  ecs_entity_t other_e) {
  flecs::world ecs(world);
  if (ecs.entity(user_e).has<UplWarp>()) {
    auto other = ecs.entity(other_e);
    if (auto other_parent = other.parent()) {
      if (other_parent.has<UplShooter>()) {
        // YAY
        // TODO: Unimplemented
      }
    }
  }
};

void projectile_contact(ecs_world_t *world, ecs_entity_t user_e,
                        ecs_entity_t other_e) {
  (void)other_e;
  flecs::world ecs(world);
  auto user = ecs.entity(user_e);

  if (!user.is_valid() || !user.has<UplProjectile>()) {
    return;
  }

  // Remove the entity when it collides with anything
  // ecs.defer_begin();
  // user.destruct();
  // ecs.defer_end();
}

bool create_shooter_components(ecs_world_t *world, ecs_entity_t e,
                               const char *source_path, const cgltf_node *node,
                               json_object *extra) {
  (void)source_path;
  flecs::world ecs(world);

  if (!node || !extra) {
    return true;
  }

  auto json_id = json_object_object_get(extra, "id");
  if (!json_id) {
    return true;
  }

  const char *id_str = json_object_get_string(json_id);
  auto ent = ecs.entity(e);

  if (SDL_strcmp(id_str, "shooter") == 0) {
    UplShooter comp = {};
    if (auto proj_obj = json_object_object_get(extra, "projectile")) {
      if (auto name_obj = json_object_object_get(proj_obj, "name")) {
        comp.prefab_name = json_object_get_string(name_obj);
      }
    }
    ent.set<UplShooter>(comp);
  } else if (SDL_strcmp(id_str, "projectile") == 0) {
    // Tag the entity as being a projectile
    ent.add<UplProjectile>();
  }

  if (auto warp_obj = json_object_object_get(extra, "shooter_tele_target")) {
    if (auto warp_str = json_object_get_string(warp_obj)) {
      UplWarp comp = {};
      comp.target_name = warp_str;
      ent.set<UplWarp>(comp);
    }
  }

  return true;
}

void post_load_shooter_components(ecs_world_t *world, ecs_entity_t e) {
  flecs::world ecs(world);

  auto phys_sys = ecs.get_mut<TbPhysicsSystem>();
  auto entity = ecs.entity(e);

  if (entity.has<UplShooter>()) {
    auto shooter = entity.get_mut<UplShooter>();
    shooter->projectile_prefab = ecs.lookup(shooter->prefab_name);
  }
  if (entity.has<UplWarp>()) {
    tb_phys_add_contact_callback(phys_sys, e, warp_trigger);
  }
}

void remove_shooter_components(ecs_world_t *world) {
  flecs::world ecs(world);
  ecs.remove_all<UplShooter>();
}

void projectile_lifetime_tick(flecs::iter &it, UplProjectile *projectiles) {
  ZoneScopedN("Projectile Expiration");
  auto ecs = it.world();

  for (auto i : it) {
    auto ent = it.entity(i);
    if (!ent.is_valid()) {
      continue;
    }
    auto &proj = projectiles[i];

    proj.lifetime -= it.delta_time();
    if (proj.lifetime <= 0.0f) {
      ent.destruct();
    }
  }
}

void shooter_tick(flecs::iter &it, UplShooter *shooters,
                  TbTransformComponent *transforms) {
  (void)transforms;
  ZoneScopedN("Shooter Update Tick");
  auto ecs = it.world();

  static float timer = 0.0f;
  timer += it.delta_time();

  auto input_sys = ecs.get_mut<TbInputSystem>();
  auto phys_sys = ecs.get_mut<TbPhysicsSystem>();

  float3 dir = {};
  // Determine if there was an input and if so what direction it should be
  // spawned relative to the player
  if (input_sys->mouse.left) {
    // TODO: How to convert mouse position into direction relative to player
  }
  if (input_sys->gamepad_count > 0) {
    auto stick = input_sys->gamepad_states[0].right_stick;
    dir = -tb_f3(stick.x, 0, stick.y);
  }
  if (tb_magf3(dir) < 0.0001f) {
    return;
  }
  dir = tb_normf3(dir);

  for (auto i : it) {
    auto entity = it.entity(i);
    auto &shooter = shooters[i];

    if (timer - shooter.last_fire_time >= 0.1f) {
      shooter.last_fire_time = timer;
    } else {
      continue;
    }

    // Spawn the projectile by cloning the prefab entity
    auto prefab = ecs.entity(shooter.projectile_prefab);
    flecs::entity mesh;
    prefab.children([&](flecs::entity child) {
      if (child.has<TbMeshComponent>()) {
        mesh = child;
      }
    });

    auto pos = tb_transform_get_world_trans(ecs.c_ptr(), entity).position;

    // Add a copy of the mesh entity as a child
    {
      auto clone = ecs.entity()
                       .is_a(shooter.projectile_prefab)
                       .enable()
                       .override(ecs_id(TbRigidbodyComponent));
      // Must manually create a unique rigid body
      {
        auto jolt = (JPH::PhysicsSystem *)(phys_sys->jolt_phys);
        auto &bodies = jolt->GetBodyInterface();

        float radius = 0.25f;
        auto shape = JPH::SphereShapeSettings(radius).Create().Get();

        // TODO: This sucks, we shouldn't have to care about impl details here
        JPH::BodyCreationSettings settings(
            shape, JPH::Vec3(pos.x, pos.y, pos.z), JPH::Quat::sIdentity(),
            JPH::EMotionType::Dynamic, Layers::MOVING);
        settings.mUserData = (uint64_t)clone.raw_id();

        auto body =
            bodies.CreateAndAddBody(settings, JPH::EActivation::DontActivate);
        TbRigidbodyComponent comp = {body.GetIndexAndSequenceNumber()};
        clone.set<TbRigidbodyComponent>(comp);

        clone.set<UplProjectile>({.lifetime = 15.0f});
        tb_phys_add_contact_callback(phys_sys, clone.raw_id(),
                                     projectile_contact);

        // Now apply velocity manually
        float3 vel = dir * 15.0f;
        bodies.SetPosition(body, JPH::Vec3(pos.x, pos.y, pos.z),
                           JPH::EActivation::Activate);
        bodies.SetLinearAndAngularVelocity(body, JPH::Vec3(vel.x, vel.y, vel.z),
                                           JPH::Vec3(0, 0, 0));
      }

      ecs.entity().is_a(mesh).child_of(clone).enable();
    }
  }
}

void upl_register_shooter_system(TbWorld *world) {
  flecs::world ecs(world->ecs);

  TbAssetSystem asset = {
      .add_fn = create_shooter_components,
      .post_load_fn = post_load_shooter_components,
      .rem_fn = remove_shooter_components,
  };
  struct ShooterAssetSystem {};
  ecs.singleton<ShooterAssetSystem>().set(asset);

  ecs.system<UplShooter, TbTransformComponent>("Shooter System")
      .kind(EcsOnUpdate)
      .iter(shooter_tick);

  ecs.system<UplProjectile>("Projectile Lifetime System")
      .kind(EcsOnUpdate)
      .iter(projectile_lifetime_tick);
}

void upl_unregister_shooter_system(TbWorld *world) {
  auto ecs = world->ecs;
  (void)ecs;
}
