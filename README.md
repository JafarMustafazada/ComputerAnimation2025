# OGLPROJs
- [OGLPROJs](#)
	- [Description](#description)
		- [Project 1](#project-1)
		- [Project 2](#project-2)
	- [Requirments](#requirments)
	- [How To Build](#how-to-build)
		- [Win32](#build-in-windows)
		- [Linux](#build-in-linux)
	- [How To Use](#how-to-use)

## Description

For computer animation course homeworks. 

### Project 1. 

Motion controlling example in c++ using modern opengl libraries.

### Project 2.

Motion of an articulated figure.

## Requirments

For building purpose.

- `cmake`
- `c++ compiler` (like Visual Studio 17 2022 or g++)
- `X11` (on linux platform only).

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

## How to Use

Here is command options:

- -ot Orientation type: quat/quaternion/0 (default), euler/1
- -it Interpolation type: crspline/catmullrom/0 (default), bspline/1
- -kf Keyframes, format: "x,y,z:e1,e2,e3;..." (Euler angles in degrees)
- -h Show this help message\n";

Project 1 - specific:
- [-m File_path], loads models with `.obj` extension (default: cube or `teapot.obj` file if it exist)

Project 2 - specific:
- [-parts torso.obj,upperL.obj,lowerL.obj,upperR.obj,lowerR.obj]
