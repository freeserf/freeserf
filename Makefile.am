
# freeserf
bin_PROGRAMS = freeserf

freeserf_SOURCES = \
	src/freeserf.cc src/freeserf.h \
	src/data.cc src/data.h \
	src/resource.h \
	src/mission.cc src/mission.h \
	src/game.cc src/game.h \
	src/serf.cc src/serf.h \
	src/flag.cc src/flag.h \
	src/building.cc src/building.h \
	src/random.cc src/random.h \
	src/pathfinder.cc src/pathfinder.h \
	src/gfx.cc src/gfx.h \
	src/viewport.cc src/viewport.h \
	src/minimap.cc src/minimap.h \
	src/interface.cc src/interface.h \
	src/gui.cc src/gui.h \
	src/popup.cc src/popup.h \
	src/game-init.cc src/game-init.h \
	src/notification.cc src/notification.h \
	src/panel.cc src/panel.h \
	src/map.cc src/map.h \
	src/player.cc src/player.h \
	src/video-sdl.cc src/video-sdl.h \
	src/video.cc src/video.h \
	src/audio.cc src/audio.h \
	src/savegame.cc src/savegame.h \
	src/log.cc src/log.h \
	src/misc.h \
	src/debug.cc src/debug.h \
	src/freeserf_endian.h \
	src/version.cc src/version.h src/version-vcs.h \
	src/data-source-dos.cc src/data-source-dos.h\
	src/tpwm.cc src/tpwm.h \
	src/event_loop.cc src/event_loop.h \
	src/event_loop-sdl.cc src/event_loop-sdl.h \
	src/sfx2wav.cc src/sfx2wav.h \
	src/xmi2mid.cc src/xmi2mid.h \
	src/data-source.cc src/data-source.h \
	src/objects.h src/smart_ptr.h \
	src/inventory.cc src/inventory.h \
	src/text-input.cc src/text-input.h \
	src/map-generator.cc src/map-generator.h

AM_CFLAGS = $(SDL2_CFLAGS) -I$(top_builddir)/src
AM_CXXFLAGS = $(SDL2_CFLAGS) -I$(top_builddir)/src
freeserf_LDADD = $(SDL2_LIBS) $(SDL2_CFLAGS) -lm

if ENABLE_SDL2_MIXER
AM_CFLAGS += $(SDL2_mixer_CFLAGS)
AM_CXXFLAGS += $(SDL2_mixer_CFLAGS)
freeserf_LDADD += $(SDL2_mixer_LIBS)
freeserf_SOURCES += src/audio-sdlmixer.cc src/audio-sdlmixer.h
else
freeserf_SOURCES += src/audio-dummy.cc src/audio-dummy.h
endif

VCS_VERSION_FILE = src/version-vcs.h

CLEANFILES = $(VCS_VERSION_FILE)


all: gitversion

.PHONY: gitversion
gitversion:
	@( FREESERF_VERSION="" ; \
	if [ ! -d $(top_srcdir)/.git ] ; then \
		echo "Git repository not found, not updating version file." ; \
	else \
		[ -e "$(VCS_VERSION_FILE)" ] && FREESERF_VERSION=$$(grep "define VERSION_VCS " "$(VCS_VERSION_FILE)" | cut -f3 -d" " | sed 's/"//g') ; \
		VCS_VERSION=$$(git describe --always --tags --dirty) ; \
		echo "FREESERF_VERSION = $$FREESERF_VERSION" ; \
		echo "VCS_VERSION = $$VCS_VERSION" ; \
		if [ "$${VCS_VERSION}" != "$${FREESERF_VERSION}" ] ; then \
			$(MKDIR_P) $(@D) ; \
			echo "Updating version file." ; \
			printf "#ifndef VERSION_VCS_H\n#define VERSION_VCS_H\n\n#define VERSION_VCS \"$${VCS_VERSION}\"\n\n#endif /* VERSION_VCS_H */\n" > $(VCS_VERSION_FILE) ; \
		fi ; \
	fi )

$(VCS_VERSION_FILE): gitversion

dist-osx:
	$(MAKE) all
	VERSION=$(VERSION) sh ./mac/deploy.sh ./freeserf FreeSerf
