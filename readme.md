# PanelMaster

A lightweight, portable parametric CAD application for electrical control panel layout design and documentation.  
Visit [PanelMaster.app](https://panelmaster.app).

Features:

- Design electrical control panels across multiple mounting surfaces
- Parametric constraints and real-world dimensions
- Position, align, duplicate, and nest components
- Quickly modify existing designs as requirements change
- Replace entire panels with automatic dimension adjustment
- Create technical documentation and reusable title blocks
- Export panel cutouts to DXF
- Export drawings to SVG
- Built-in component library
- Documentation and help

## Build / Installation

### Native (Linux)

- Make sure **python**, **gcc**, and **make** are installed
- Install **raylib** (preferably static). See the [official raylib build guide](https://github.com/raysan5/raylib/wiki/Working-on-GNU-Linux) for installation instructions.
- Run `make` to build PanelMaster

### Native (Windows)

PanelMaster is built using the **MSYS2 MinGW-w64** environment.

- Install [MSYS2](https://www.msys2.org/)
- Update the system by running `pacman -Syu`, restarting the terminal when prompted, and running it again
- Install GCC running `pacman -S mingw-w64-x86_64-gcc`
- Install Make running `pacman -S mingw-w64-x86_64-make`
- Download [raylib 6.0 for MinGW-w64](https://github.com/raysan5/raylib/releases/download/6.0/raylib-6.0_win64_mingw-w64.zip)
- Copy the contents of `lib` and `include` into the corresponding directories under `C:\msys64\mingw64`
- Run `make` to build PanelMaster

## Components

This repository includes a small selection of components as examples.
A larger component library, along with a precompiled Windows binary, is available on my [Ko-fi](https://ko-fi.com/s/8372ad10ad).

## Thanks

PanelMaster is made possible thanks to the following outstanding open-source projects and their developers:

- **raylib** by raysan5 – https://www.raylib.com/
- **cJSON** by Dave Gamble – https://github.com/DaveGamble/cJSON
- **dxfout** – https://hackaday.io/project/165083-a-simple-dxf-output-library
- **PDFGen** by Andre Renaud – https://github.com/AndreRenaud/PDFGen
- **ttf2mesh** by fetisov – https://github.com/fetisov/ttf2mesh
- **svg-library-c** by CodeDrome – https://github.com/CodeDrome/svg-library-c
- **Origami** by SuckDuck – https://github.com/SuckDuck/origami