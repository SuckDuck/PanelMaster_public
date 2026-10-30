#! /usr/bin/python
from origami.littlebuild.littlebuild import *
import origami.build as origami
import sys, shutil

project_name = "panelmaster"
project_version = "BETA1.3.0"

CC          = "gcc"
BUILD_DIR   = "build"
DIST_DIR    = "dist"
ASSETS_DIR  = "assets"
STATICS_DIR = "statics"
LIB_PATHS   = []

CFLAGS =  [
    "-g", 
    "-O0"
]

SRC_DIRS = [
    "src", 
    "statics",
    "origami/src", 
    "origami/statics", 
    "libs/cjson", 
    "libs/pdfgen", 
    "libs/ttf2mesh",
    "libs/dxfout",
    "libs/svg"
]

INCLUDES = [
    "include", 
    "statics",
    "origami/include", 
    "origami/statics", 
    "libs/cjson", 
    "libs/pdfgen", 
    "libs/ttf2mesh",
    "libs/dxfout",
    "libs/svg"
]

MACROS = {
    "PROJECT_TITLE":f'"{project_name} v{project_version}"',
    "PROJECT_VERSION": f'"{project_version.replace(".","_")}"',
    "OG_ZOOM_SPEED":"0.02f",
    "OG_MIN_PANEL_SIZE":"10"
}

def _win_compile_icon(icon, out_dir, out_name):
    rc = f"{out_dir}/{out_name}.rc"
    obj = f"{out_dir}/{out_name}.o"

    print(f"{COLORS.GREEN}Compiling icon:{COLORS.RESET} {icon}")

    rel_icon = os.path.relpath(icon, out_dir)

    with open(rc, "w") as f:
        f.write(f'IDI_ICON1 ICON "{rel_icon}"\n')

    try:
        subprocess.run(
            ["windres", rc, "-O", "coff", "-o", obj],
            check=True
        )
    except subprocess.CalledProcessError:
        print(
            f"{COLORS.RED}Error:{COLORS.RESET} Failed compiling icon: {icon}",
            file=sys.stderr
        )
        raise

    os.remove(rc)

    return obj

def clean():
    origami.clean()
    rm_all(BUILD_DIR)
    rm_all(STATICS_DIR)

def statics():
    origami.statics()
    create_dirs(["statics"])
    create_statics(STATICS_DIR, [
        os.path.join(ASSETS_DIR, "Icon.png")
    ])

def linux(aditional_macros:Dict):
    LINKS = ["-lX11","-lm","-lraylib", "-lGL"]
    LDFLAGS = []
    
    create_dirs([BUILD_DIR])
    all_srcs = [f"{d}/{f}" for d in SRC_DIRS for f in os.listdir(d) if f.endswith(".c")]
    objs = compile(CC, CFLAGS, MACROS | aditional_macros, INCLUDES, all_srcs, BUILD_DIR)
    link(CC, objs, LIB_PATHS, LINKS, LDFLAGS, BUILD_DIR, f'"{project_name}"')

def windows(aditional_macros:Dict):
    LINKS = ["-lraylib", "-lgdi32", "-lwinmm"]
    LDFLAGS = ["-mwindows"]

    create_dirs([BUILD_DIR])
    all_srcs = [f"{d}/{f}" for d in SRC_DIRS for f in os.listdir(d) if f.endswith(".c")]
    objs = [_win_compile_icon(os.path.join(ASSETS_DIR, "Icon.ico"), BUILD_DIR, "win_icon")]
    objs = objs + compile(CC, CFLAGS, MACROS | aditional_macros, INCLUDES, all_srcs, BUILD_DIR)
    link(CC, objs, LIB_PATHS, LINKS, LDFLAGS, BUILD_DIR, f'"{project_name}"')


if __name__ == "__main__":
    if   "clean" in sys.argv: clean(); exit(0)
    elif "clear" in sys.argv: clean(); exit(0)
    
    statics()
    if "statics" in sys.argv: exit(0)

    if "web" in sys.argv: web({"ROOT":'"."'}); exit(0)
    elif sys.platform.startswith("linux") : linux({"ROOT": '"."' if "dist" in sys.argv else '".."'})
    elif sys.platform.startswith("cygwin"): windows({"ROOT": '"."' if "dist" in sys.argv else '".."'})
    elif sys.platform.startswith("win32") : print("Please run this build using MSYS2-MINGW64 with MinGW-w64 GCC"); exit(1)
    else : print("Non supported platform"); exit(1)

