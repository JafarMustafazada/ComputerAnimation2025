# **Rigid Body Physics System – Short Documentation (`oglproj3.h`)**

# **1. Overview**

The rigid body system implements **basic physical motion** for spherical objects in a 3D OpenGL scene.
It simulates:

* Gravity
* Linear and angular motion
* Sphere–sphere collisions
* Sphere–ground collisions
* Simple rotational dynamics
* Rendering of moving rigid bodies

The system is self-contained and does not depend on external physics libraries (glm being used just for math).

# **2. Rigid Body Representation**

Each dynamic object is represented by a `RigidBody` struct:

## **Stored Physical Quantities**

| Property          | Purpose                                                |
| ----------------- | ------------------------------------------------------ |
| `position`        | World-space location of the sphere center              |
| `velocity`        | Linear velocity                                        |
| `orientation`     | Quaternion orientation of the sphere                   |
| `angularVelocity` | Rotation speed around body axes                        |
| `radius`          | Sphere collision size                                  |
| `mass`, `invMass` | Supports dynamic & static objects (`mass=0 -> static`) |
| `invInertia`      | Approx inverse inertia for a solid sphere              |
| `restitution`     | Bounciness parameter (0 = dead, 1 = perfectly elastic) |

Rigid bodies use **solid-sphere moment of inertia**:

$$[
I = \frac{2}{5} m r^2
]$$

## **Model Matrix Generation**

```cpp
model = translate(position) * mat4_cast(orientation) * scale(radius);
```

# **3. Physics Engine Structure**

The physics engine is encapsulated in a single class:

```cpp
class PhysicsEngine {
    std::vector<RigidBody> bodies;
    glm::vec3 gravity;
    float groundY;
};
```

The engine is responsible for:

1. **Integrating motion**
2. **Detecting collisions**
3. **Resolving collisions with impulses**
4. **Correcting penetration**
5. **Updating orientation from angular velocity**

The Application advances physics from its `update()` function step:

```cpp
physics.step(dt);
```

# **4. Motion Integration**

The engine uses **semi-implicit Euler integration**, which is stable and cheap:

**Linear:**

```cpp
velocity += gravity * dt;
position += velocity * dt;
```

**Angular:**

```cpp
orientation += 0.5 * (orientation * (0,angularVelocity)) * dt;
normalize(orientation);
```

# **5. Collision System**

## **5.1 Sphere–Sphere Collision Detection**

Two bodies collide if:

$$[
||p_B - p_A|| < r_A + r_B
]$$

If penetration exists:

* Compute contact normal
* Compute relative velocity
* Apply impulse
* Apply angular change
* Correct interpenetration

## **5.2 Impulse Resolution**

Impulse ( j ) is computed by:

$$[
j = \frac{-(1+e)(v_{rel}\cdot n)}{\frac{1}{m_A} + \frac{1}{m_B} + \text{rotational term}}
]$$

Where:

* ( e = ) restitution (bounciness)
* ( n = ) collision normal

Applied as:

```cpp
A.velocity -= j * n * A.invMass;
B.velocity += j * n * B.invMass;
```

Rotational effects are applied using:

```cpp
angularVelocity += invInertia * cross(r, impulse);
```

## **5.3 Penetration Correction**

To prevent sinking:

```cpp
correction = penetration * percent * normal;
A.position -= correction * A.invMass;
B.position += correction * B.invMass;
```

## **5.4 Ground Collision**

A simple ground plane is used:

```
if (position.y - radius < groundY)
```

* Push sphere above ground
* Reflect vertical velocity using restitution
* Add slight friction damping in XZ

# **6. Rendering of Physics Objects**

The Application (from `main3.cpp`) creates a sphere mesh once:

```cpp
sphereMesh = GeometryFactory::createSphere(...);
```

Then for each body:

```cpp
glm::mat4 model = b.modelMatrix();
renderMesh(*sphereMesh, model);
```

This allows all rigid bodies to be visible and animated within the scene.

# **7. Integration With Application**

### **Initialization**

```cpp
app.createPhysicsScene();
```

The scene creates several spheres with:

* Different sizes
* Different masses
* Random positions and velocities

### **Update Loop**

`Application::update()` advances both:

* MotionController time
* Physics engine step

### **Render Loop**

`Application::render()` draws:

1. Scene mesh (teapot or model) (doesn't have rigid body)
2. Physics objects (rigid spheres)