# Origami

Origami is a multi-viewport UI framework in C for building editor tools, built on top of [raylib](https://www.raylib.com/) and [microui](https://github.com/rxi/microui).

![demo](media/demo.gif)

Features:
* Multiple independent viewports  
* Viewport grouping with resizable splits
* Show/hide, dragging and resizing support for viewports  
* Z-order viewport stacking  
* Modal viewports
* Per-viewport zoom and pan
* Custom overlay and underlay rendering per viewport  
* Per-viewport UI via microUI
* Per-viewport argc/argv-style command interface  
* On-screen logging  
* Callback-based API (update, render, UI, etc.)  
* Built-in file dialog
* Built-in context menu
* Built-in input dialog

## Build

Origami depends on raylib and requires it to be installed. Static linking is recommended.  
When cloning this repo, make sure to also initialize and update submodules:
```bash
git clone --recurse-submodules https://github.com/SuckDuck/origami
```

To build everything, run the following:

```bash
make         # Build the resources
make example # Build the example - optional
```

Integrating Origami into another project:
1. Add Origami as a git submodule
1. Build the resources as above
1. Include `src` in your list of source files.
1. Add `include` and `statics` to your include paths.

<br>

Origami includes [LittleBuild](https://github.com/SuckDuck/littlebuild) as a submodule, which is used to build the resources and the example. You can also use it to compile your own project. If you do, you can import the origami build functions like this:

```python
import origami.build as origami
```

Then you can build Origami alongside your project from the same place.

## Usage

Before using Origami, it’s recommended to read the [microui usage guide](https://github.com/rxi/microui/blob/master/doc/usage.md).  
Also, see the example in this repository.

Origami has three main components: Command Bar, Viewports, and Layouts.

- **Command Bar** – A small command-line interface opened with a shortcut (Shift + Enter by default).  
  It is used to run commands and, initially, to open viewports.

- **Viewports** – Independent windows. They are the **central part of Origami** and consist of four components:  
  1. **Logic callbacks** – Functions such as `Init()` and `Update()` that define viewport behavior.  
  2. **Render callbacks** – `Render()`, `RenderOverlay()`, and `RenderUnderlay()`.  
  3. **UI callback** – Used to place `microUI` widgets inside viewports.  
  4. **Command interface** – An `argc/argv`-style interface triggered from the Command Bar. Each viewport can define custom commands.

- **Layouts** – The system used to group multiple viewports into a single unified structure with resizable split regions.
  
<br>

The general usage workflow looks like this:

- Register your viewports and their callbacks using `OG_InitViewport()`.  
It’s recommended to wrap this in a helper function for each viewport, as the function signature is long.  
- Optionally, build layouts using `OG_InitLayout()` and related layout functions.  
  Layouts allow you to group multiple viewports into a single structure.
- Call `OG_Init()` after registering all viewports.  
- In your main loop, call `OG_Update()`.  
- Call `OG_Free()` at the end of your application.

So your whole `main()` function can look like this:

```C
int main(int argc, char **argv){
    
    // Viewports and layouts init functions
    Viewport0();
    Viewport1();
    
    OG_Init("origami_example", 60);
    while (true) {
        if (OG_Update())
            break;
    }

    OG_Free();
    return 0;
}
```

For an example of how to register a viewport and use its callbacks, please see the [example included in this repository](example/).