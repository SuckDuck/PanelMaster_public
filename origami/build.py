#! /usr/bin/python
try:
    from .littlebuild.littlebuild import *
except ImportError:
    from littlebuild.littlebuild import *
import os

project_title = "example"
project_root = str(Path(__file__).parent)

def statics():
    ASSETS_DIR = os.path.join(project_root,"assets")
    STATICS_DIR = os.path.join(project_root,"statics")
    create_dirs([STATICS_DIR])

    input_files = [str(Path(ASSETS_DIR) / f) for f in os.listdir(ASSETS_DIR)]
    create_statics(STATICS_DIR,input_files)

def example():
    CC = "gcc"
    CFLAGS =  ["-g", "-O0"]
    BUILD_DIR = f"{project_root}/build"
    SRC_DIRS = [
        os.path.join(project_root,"src"),
        os.path.join(project_root,"example")
    ]
    all_srcs = [f"{d}/{f}" for d in SRC_DIRS for f in os.listdir(d)]
    
    INCLUDES = [
        os.path.join(project_root, "include"),
        os.path.join(project_root, "statics")
    ]
    
    LIB_PATHS = []
    LINKS = ["-lX11","-lm","-lraylib"]
    LDFLAGS = []
    MACROS = {}
    
    create_dirs([BUILD_DIR])
    objs = compile(CC,CFLAGS,MACROS,INCLUDES,all_srcs,out_dir=BUILD_DIR)
    link(CC,objs,LIB_PATHS,LINKS,LDFLAGS,BUILD_DIR,project_title)

def clean():
    rm_all(os.path.join(project_root, "build"))
    rm_all(os.path.join(project_root, "statics"))


if __name__ == "__main__":
    if "clean"   in sys.argv: clean();   exit(0)
    if "clear"   in sys.argv: clean();   exit(0)
    if "example" in sys.argv: example(); exit(0)
    statics()
