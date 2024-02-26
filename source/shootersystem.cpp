#include "projectilecomponent.h"
#include "shootercomponent.h"

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

void projectile_contact(ecs_world_t *world, ecs_entity_t user_e,
                        ecs_entity_t other_e) {
  (void)other_e;
  flecs::world ecs(world);
  auto user = ecs.entity(user_e);

  if (!user.is_valid() || !user.has<UplProjectile>()) {
    return;
  }

  ecs.defer_begin();
  // Set the user projectile lifetime so that it will begin ticking down
  // and when it hits 0, be destroyed
  tb_auto proj = user.get_mut<UplProjectile>();
  if (proj->lifetime <= 0.0) {
    proj->lifetime = 1.5f;
  }
  user.modified<UplProjectile>();
  ecs.defer_end();
}

void projectile_lifetime_tick(flecs::iter &it, UplProjectile *projectiles) {
  ZoneScopedN("Projectile Expiration");
  auto ecs = it.world();

  ecs.defer_begin();
  for (auto i : it) {
    auto ent = it.entity(i);
    if (!ent.is_valid()) {
      continue;
    }
    auto &proj = projectiles[i];
    if (proj.lifetime <= 0.0) {
      continue;
    }

    proj.lifetime -= it.delta_time();
    if (proj.lifetime <= 0.0f) {
      proj.lifetime = 0.0f;

      ent.destruct();
    }
  }
  ecs.defer_end();
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

        clone.set<UplProjectile>({.lifetime = 0.0f});
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

extern "C" {
void upl_register_shooter_sys(TbWorld *world) {
  flecs::world ecs(world->ecs);

  ecs.system<UplShooter, TbTransformComponent>("Shooter System")
      .kind(EcsOnUpdate)
      .iter(shooter_tick);

  ecs.system<UplProjectile>("Projectile Lifetime System")
      .kind(EcsOnUpdate)
      .iter(projectile_lifetime_tick);
}

void upl_unregister_shooter_sys(TbWorld *world) {
  auto ecs = world->ecs;
  (void)ecs;
}

TB_REGISTER_SYS(upl, shooter, TB_SYSTEM_NORMAL);
}
