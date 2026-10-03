.PHONY: all statics dist clean clear

all:
	./build.py || /usr/bin/python ./build.py

statics:
	./build.py statics || /usr/bin/python ./build.py statics
	

clean:
	./build.py clean || /usr/bin/python ./build.py clean

clear:clean
