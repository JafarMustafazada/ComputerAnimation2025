# OGLPROJs
- [OGLPROJs](#)
	- [Description](#description)
		- [Project 1](#project-1)
		- [Project 2](#project-2)
		- [Project 3](#project-3)
		- [Project 4](#project-4)
	- [Requirments](#requirments)
	- [How To Build](#how-to-build)
		- [Win32](#build-in-windows)
		- [Linux](#build-in-linux)
	- [How To Use](#how-to-use)
		- [Project 1 Adds](#project-1-adds)
		- [Project 2 Adds](#project-2-adds)
		- [Project 3 Adds](#project-3-adds)
		- [Project 4 Adds](#project-4-adds)

---

## Description

For computer animation course homeworks. 

### Project 1. 

Motion controlling example in c++ using modern opengl libraries.

### Project 2.

Motion of an articulated figure.

### Project 3.

Rigid-body motion with gravity, sphere to sphere and sphere to floor collisions.

### Project 4.

Reynolds-like behavioral flock (boids) system.

---

## Requirments

For building purpose.

- `cmake`
- `c++ compiler` (like Visual Studio 17 2022 or g++)
- `X11` (on linux platform only).

---

## How to Build

It is as easy as pressing one button (build/run) if you have `visual studio code` with cmake extension (https://marketplace.visualstudio.com/items?itemName=ms-vscode.cmake-tools).

Optionally you can un-comment line 16 in `CMakeLists.txt` in order to get binaries straight in `bin/` (instead of `out/build/preset_name/config_name/`) folder for convinience.

Also you can edit `CMakePresets.json` to work with any other compiler (`visual studio` and `gcc` being already configured) and settings.

### Build in Windows

To build project, first generate the build files with command:

- `cmake --preset msvc1`

Then build with (will put binary in `"path/to/project/out/build/msvc1/Release/binary.exe"`):

- `cmake --build --preset msvc1-release`

### Build in Linux

Install tools:

- `sudo apt update`
- `sudo apt install -y build-essential cmake pkg-config git libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxfixes-dev libxi-dev libgl1-mesa-dev libudev-dev libxkbcommon-dev`

To build project, first generate the build files with command:

- `cmake --preset gnu1-release`

Then build with (will put binary in `"path/to/project/out/build/gnu1-release/bin/binary"`):

- `cmake --build out/build/gnu1-release`

---

## How to Use

Use whatever header files you need, each made for to improve aviable features using previus one.

Each `cpp` project `N` have commands from project `N-1`.

### Project 1 Adds

-  -ot \<type>               Orientation type: quaternion|0 or euler|1 (default: quaternion)
-  -it \<type>               Interpolation type: catmullrom|0 or bspline|1 (default: catmullrom)
-  -kf \<kf1;kf2;...>        Keyframes in format x,y,z:e1,e2,e3 separated by semicolons
-  -fn \<filename>           Additional model filename to load (OBJ format)
-  -h, --help                Show this help message

### Project 2 Adds

-  -articulated             Enable articulated figure rendering (requires 5 meshes)
- example: -fn data/n1.obj, -fn data/n2.obj, -fn data/n3.obj, -fn data/nr.obj, -fn data/n5.obj.
- articulated figure order: torso, left thigh, left shin, right thigh, right shin.

### Project 3 Adds

-  -seed \<number>           Seed for random number generator in physics scene (default: 12345)
-  -physicscene \<N>         Create physics scene with N spheres (default: 6)

### Project 4 Adds

-  -flock \<N>                Create flocks with N boids (default: 48)