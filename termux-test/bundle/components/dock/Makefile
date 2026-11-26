# Fallback build for environments without meson (e.g. this dev VM).
# The canonical build is meson.build; this Makefile mirrors it.
CC      ?= gcc
CFLAGS  ?= -std=c11 -Wall -Wextra -O2 -g
PKGS     = wayland-client cairo pangocairo glib-2.0 gobject-2.0
CFLAGS  += $(shell pkg-config --cflags $(PKGS)) -Ibuild
LDLIBS   = $(shell pkg-config --libs $(PKGS)) -lm

PROTOS   = wlr-layer-shell-unstable-v1 \
           wlr-foreign-toplevel-management-unstable-v1 \
           xdg-popup-types
GEN_H    = $(foreach p,$(PROTOS),build/$(p)-client-protocol.h)
GEN_C    = $(foreach p,$(PROTOS),build/$(p)-protocol.c)

SRC      = src/main.c src/config.c src/draw.c src/apps.c src/magnify.c \
           src/toplevel.c

all: build/mux-dock

build:
	mkdir -p build

build/%-client-protocol.h: protocol/%.xml | build
	wayland-scanner client-header $< $@

build/%-protocol.c: protocol/%.xml | build
	wayland-scanner private-code $< $@

build/mux-dock: $(SRC) $(GEN_H) $(GEN_C)
	$(CC) $(CFLAGS) -o $@ $(SRC) $(GEN_C) $(LDLIBS)

build/test-magnify: tests/test_magnify.c src/magnify.c
	$(CC) $(CFLAGS) -o $@ $^ -lm

build/test-apps: tests/test_apps.c src/apps.c
	$(CC) $(CFLAGS) -o $@ $^ $(shell pkg-config --libs glib-2.0 gobject-2.0)

build/test-config: tests/test_config.c src/config.c
	$(CC) $(CFLAGS) -o $@ $^ $(shell pkg-config --libs glib-2.0 gobject-2.0)

test: build/test-magnify build/test-apps build/test-config build/test-draw
	./build/test-magnify
	./build/test-apps
	./build/test-config
	./build/test-draw

clean:
	rm -rf build

.PHONY: all test clean

build/test-draw: tests/test_draw.c src/draw.c src/config.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)
