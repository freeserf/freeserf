
# freeserf
bin_PROGRAMS = freeserf

freeserf_SOURCES = \
	src/freeserf.c src/freeserf.h \
	src/globals.h \
	src/serf.c src/serf.h \
	src/flag.c src/flag.h \
	src/building.c src/building.h \
	src/random.c src/random.h \
	src/gfx.c src/gfx.h \
	src/viewport.c src/viewport.h \
	src/map.c src/map.h \
	src/player.c src/player.h \
	src/sdl-video.c src/sdl-video.h \
	src/list.c src/list.h \
	src/misc.h \
	src/debug.h \
	src/freeserf_endian.h \
	src/version.h

AM_CFLAGS = $(SDL_CFLAGS) -I$(top_builddir)/src
freeserf_LDADD = $(SDL_LIBS) $(SDL_CFLAGS) -lm

#CLEANFILES = src/version.h

#.PHONY: src/version.h
#src/version.h: Makefile
#	$(MKDIR_P) $(@D)
#	$(top_srcdir)/create-version.sh $@
