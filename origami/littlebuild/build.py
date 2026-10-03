#! /usr/bin/python
from littlebuild import *

def all():
    CC = "gcc"
    CFLAGS =  []
    SOURCES = ["example.c"]
    OUTPUT = "example"
    BUILD_DIR = "build"
    INCLUDES = []
    LIB_PATHS = []
    LINKS = []
    LDFLAGS = []
    MACROS = {}
    
    create_dirs([BUILD_DIR])
    objs = compile(CC,CFLAGS,MACROS,INCLUDES,SOURCES,out_dir=BUILD_DIR)
    link(CC, objs, LIB_PATHS, LINKS, LDFLAGS, BUILD_DIR, OUTPUT)

def clean():
    rm_all("build")

if __name__ == "__main__":
    if   "clean" in sys.argv: clean()
    elif "clear" in sys.argv: clean()
    else: all()