## Particle System with Perlin Noise Documentation (oglproj5.h)

### Overview
This system implements a particle emitter with Perlin noise-based turbulence, physics integration, and configurable emission patterns.

### 1. **PerlinNoise Class**

**Purpose**: Generates 3D noise for turbulence.

**Key Components**:

- **Permutation Table (`p`)**: A shuffled array [0-255] duplicated to 512 elements. This provides pseudorandom but repeatable gradient lookups. The duplication eliminates boundary wrapping logic.

- **`noise(x, y, z)`**: Classic Perlin implementation
  - **Grid cell location**: `floor(x) & 255` finds which unit cube contains the point (bitwise AND handles wrapping)
  - **Fractional position**: `x - floor(x)` gives position within the cube (0 to 1)
  - **Fade curves**: `fade(t) = 6t⁵ - 15t⁴ + 10t³` provides smooth C² continuous interpolation (Perlin's improvement over linear)
  - **Gradient selection**: Uses permutation table to select 8 corner gradients
  - **Trilinear interpolation**: Three nested `lerp()` calls blend the 8 corner contributions
  - **Output range**: [-1, 1]

- **`normalizedNoise()`**: Wrapper that:
  - Scales input by frequency (`nF`)
  - Applies time-based animation to z-coordinate (`nTS`)
  - Remaps [-1,1] → [0,1] → [min,max]
  - **Note**: Parameters `nF`, `nA`, `nTS` allow octave layering for fractal Brownian motion

### 2. **Particle Structure**

Simple data container with:
- **Kinematic state**: `position`, `velocity`
- **Visual properties**: `color`, `size`, `rotation`
- **Lifecycle**: `life` (current age) vs `lifetime` (max age)
- **`alive()`**: Functional check rather than boolean flag (cleaner for culling)

### 3. **EmitterParams Configuration**

**Emission Control**:
- **Continuous mode**: `emitRate` particles/second with fractional accumulator
- **Burst mode**: Spawns `burstCount` particles instantaneously

**Particle Initialization Ranges**:
- Lifetime, size, color (start/end for gradient), velocity bounds
- `spread`: Global randomness multiplier

**Physics Parameters**:
- **`gravity`**: Constant acceleration vector (typically -Y)
- **`drag`**: Linear damping coefficient. Implementation uses `v *= 1/(1 + drag*dt)` which is semi-implicit Euler approximation of `dv/dt = -drag*v`

**Noise Configuration**:
- **`NONE`**: No turbulence
- **`UNIFORM`**: White noise (random each frame)
- **`PERLIN`**: Smooth coherent noise
- **Frequency/Amplitude**: Control noise detail and strength
- **Time scale**: Animates the noise field itself

**Spawn Volume**:
- **`POINT`**: All particles at origin
- **`SPHERE`**: Uniform distribution using power-law radius (`r^(1/3)` ensures volume uniformity)
- **`BOX`**: Uniform random in axis-aligned box

**Coordinate Systems**:
- **Local space**: Particles spawn relative to `worldTransform` (emitter can move)
- **World space**: Spawn positions are absolute

### 4. **ParticleEmitter::update() - The Core Loop**

**Step 1: Emission**
```cpp
float toEmit = params.emitRate * dt + emitAccumulator;
int count = (int)floor(toEmit);
emitAccumulator = toEmit - float(count);
```
Fractional accumulator ensures exact emission rate over time (e.g., 0.3 particles/frame accumulates to 1 every 3-4 frames).

**Step 2: Per-Particle Integration**

For each alive particle:

1. **Noise Force Calculation**:
   - **Perlin mode**: Samples 3 offset noise fields to get independent X/Y/Z components
   - Offsets (37.1, 17.3, etc.) decorrelate the axes
   - Time offset in z-coordinate animates the field
   - **Uniform mode**: Pure random vector each frame

2. **Velocity Update** (Semi-Implicit Euler):
   ```cpp
   velocity += (gravity + noise) * dt  // Acceleration
   velocity *= 1/(1 + drag*dt)         // Damping
   ```

3. **Position Update**:
   ```cpp
   position += velocity * dt  // Explicit position integration
   ```

4. **Collision Response** (turned off):
   - Sphere-sphere intersection test with physics bodies
   - **Reflection**: `v -= (1+e)*dot(v,n)*n` where `e` is restitution
   - **Penetration resolution**: Push particle outside sphere radius + epsilon

**Step 3: Particle Culling**
- Compaction algorithm removes dead particles in-place
- Avoids allocation overhead during runtime

### 5. **spawnParticle() - Initialization**

**Position Generation**:
- **Sphere**: Uses rejection-free method:
  1. Random direction (normalized random vector)
  2. Radius from power distribution (ensures uniform volume density, not surface clustering)
- **Box**: Simple uniform random in each axis

**Velocity Initialization**:
- Random within `[velocityMin, velocityMax]` bounds
- Multiplied by `spread` for global control

**Transform Application**:
- If local space, spawned position is transformed by `worldTransform` matrix
- Allows emitters attached to moving objects

### 6. **Rendering Integration**

**`renderAll()` Template**:
- Uses callback pattern to decouple particle system from graphics API
- For each particle, constructs model matrix: `T(position) * S(size)`
- Passes matrix, color, size to provided render function
- Allows same emitter to work with different renderers (OpenGL, Vulkan, etc.)

**`applyMorphs()`**:
- Interpolates color from `colorStart` to `colorEnd` based on normalized lifetime
- Should be called before rendering each frame
- Can be extended for size curves, rotation, or texture atlas animation

### 7. **Application integration (main5.cpp)**

**from `main4.cpp` changed functions**:  

- update() 
- render()
- initialize()
- keyCallback()

**Added following helper text**:

```
Particle Emitter Keyboard Controls:
  CTRL+0                   Reset: disable articulated figure, flock, and particles
  CTRL+1                   Load particle preset: Fountain
  CTRL+2                   Load particle preset: Plasma
  CTRL+3                   Load particle preset: Smoke
  CTRL+4                   Load particle preset: Snow
  CTRL+5                   Load particle preset: Fire_Long

  CTRL+Q / SHIFT+Q         Increase / decrease scale factor (for adjustments)
  CTRL+W / SHIFT+W         Increase / decrease maxParticles
  CTRL+E / SHIFT+E         Increase / decrease lifetimeMax
  CTRL+R / SHIFT+R         Increase / decrease spread
  CTRL+T / SHIFT+T         Increase / decrease sizeMin
  CTRL+Y / SHIFT+Y         Increase / decrease sizeMax

Color Start (RGBA):
  CTRL+A / SHIFT+A         Increase / decrease colorStart.r (Red)
  CTRL+S / SHIFT+S         Increase / decrease colorStart.g (Green)
  CTRL+D / SHIFT+D         Increase / decrease colorStart.b (Blue)
  CTRL+J / SHIFT+J         Increase / decrease colorStart.a (Alpha/Transparency)

Color End (RGBA):
  CTRL+F / SHIFT+F         Increase / decrease colorEnd.r (Red)
  CTRL+G / SHIFT+G         Increase / decrease colorEnd.g (Green)
  CTRL+H / SHIFT+H         Increase / decrease colorEnd.b (Blue)
  CTRL+K / SHIFT+K         Increase / decrease colorEnd.a (Alpha/Transparency)

Noise Settings:
  CTRL+Z / SHIFT+Z         Increase / decrease noiseAmplitude
  CTRL+X / SHIFT+X         Increase / decrease noiseFrequency
  CTRL+C / SHIFT+C         Increase / decrease noiseTimeScale

Particle Mesh:
  CTRL+V / SHIFT+V         Cycle through particle mesh types (Sphere/Cube/Cylinder)

Notes:
  - All CTRL combinations increase values.
  - All SHIFT combinations decrease values.
  - Color (RGBA) values are clamped between 0.0 and 1.0.
  - Values cannot go below zero.
```