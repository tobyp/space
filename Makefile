PKGCONFIG_PACKAGES=glfw3 gl glew cairo

CC=clang
CFLAGS=--std=c11 -ggdb -pedantic -Wno-unused-parameter $(shell pkg-config --cflags $(PKGCONFIG_PACKAGES))
LDLIBS=$(shell pkg-config --libs $(PKGCONFIG_PACKAGES)) -lm

space: body.o
