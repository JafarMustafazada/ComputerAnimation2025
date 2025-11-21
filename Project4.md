# Project 4 (Boids / Reynolds-like Flock)

## Description

Adds a simple Reynolds-style flocking system (boids) to the OpenGL project (by default uses simple spheres as mesh) using physics engine from project 3.
The system provides emergent flock motion through a weighted blend of classic behaviors: **separation**, **alignment**, **cohesion**, plus **wander**, **obstacle avoidance** and a soft world-confinement (**center pull**).

## Responsibilities

* **`Boid`**

  * Per-boid state: `position`, `velocity`, `acceleration`, `radius`, `maxSpeed`, `maxForce`.
  * Very small — just the runtime data for a single agent.

* **`Flock`**

  * Holds `std::vector<Boid>` and tuning parameters:

    * `neighborRadius`, `separationRadius`
    * behavior weights: `wSeparation`, `wAlignment`, `wCohesion`, `wWander`, `wAvoid`
    * wander settings (`wanderJitter`, `wanderRadius`)
    * `worldRadius` and `centerPull` for confinement
  * `update(float dt, PhysicsEngine *physicsEngine = nullptr)` — core step:

    * For each boid: find neighbors (simple O(N²) loop), compute separation/alignment/cohesion forces, add wander, obstacle avoidance from `PhysicsEngine::bodies`, combine using weighted sum, then integrate velocity and position (clamped by `maxSpeed`/`maxForce`).
  * Helpers for vector limiting and setting magnitudes.

* **Integration with `Application (main4.cpp)`**

  * New members: `std::unique_ptr<Flock> flock; std::unique_ptr<Mesh> boidMesh;`
  * Helper: `createFlock(int N = 48)` — constructs `Flock` and boid visual mesh.
  * Update loop: call `flock->update(dt, &physics);`.
  * Render loop: iterate `flock->boids` and call existing `renderMesh(*boidMesh, model)`; small sphere or cone can represent each boid. Orientation computed from velocity to face movement.

## How to enable (quickly)

Call:

```cpp
Application app(800, 600);
if (!app.initialize()) return -1;
parseIO(argc, argv, app);
app.createFlock(64);        // creates 64 boids
app.run();
```

## Tuning

* `neighborRadius = 0.9f` — how far boids look for others
* `separationRadius = 0.28f` — minimum comfortable distance
* `wSeparation = 1.8f`, `wAlignment = 1.0f`, `wCohesion = 0.9f` — relative behavior weights
* `wAvoid = 2.5f` — increase to make boids avoid physics-spheres more strongly
* `worldRadius = 3.0f` and `centerPull = 1.0f` — keep flock inside world bounds
* `maxSpeed` and `maxForce` are per-boid and randomized slightly for variety