/* audio.c */

#include "SDL.h"

#include "audio.h"
#include "gfx.h"
#include "data.h"
#include "log.h"
#include "list.h"

typedef struct {
	list_elm_t elm;
	char *data;
	int size;
	int played;
} audio_clip_t;

static list_t sfx_clips_to_play;
static int initialized = 0;

static void
audio_request_data_cb(void *user, uint8_t *stream, int len);

static void
sfx_init()
{
	list_init(&sfx_clips_to_play);

	SDL_AudioSpec desired = {
		.freq = 8000,
		.format = AUDIO_S8,
		.channels = 1,
		.samples = 256,
		.userdata = NULL,
		.callback = audio_request_data_cb
	};
	
	int r = SDL_OpenAudio(&desired, NULL);
	if (r < 0) {
		LOGE("Could not open audio device: %s\n", SDL_GetError());
		return;
	}

	initialized = 1;
}

static void
audio_request_data_cb(void *user, uint8_t *stream, int len)
{
	if (list_is_empty(&sfx_clips_to_play)) {
		SDL_PauseAudio(1);
		return;
	}
	list_elm_t *elm = NULL;
	list_foreach(&sfx_clips_to_play, elm) {
		audio_clip_t *clip = (audio_clip_t *)elm;
		int left = clip->size - clip->played;
		len = (len > left) ? left : len;
	}
	list_elm_t *tmpelm = NULL;
	list_foreach_safe(&sfx_clips_to_play, elm, tmpelm) {
		audio_clip_t *clip = (audio_clip_t *)elm;
		SDL_MixAudio(stream, (Uint8*)(clip->data+clip->played), len, SDL_MIX_MAXVOLUME);
		clip->played += len;
		if (clip->played == clip->size) {
			list_elm_remove((list_elm_t*)clip);
			free(clip);
		}
	}
}

/* Play audio. */
void
enqueue_sfx_clip(sfx_t sfx)
{
	if (0 == initialized) {
		sfx_init();
		if (0 == initialized) {
			return;
		}
	}

	audio_clip_t *audio_clip = malloc(sizeof(audio_clip_t));
	if (audio_clip == NULL) abort();

	size_t size = 0;
	audio_clip->data = gfx_get_data_object(DATA_SFX_BASE + sfx, &size);
	audio_clip->size = (int)size;
	audio_clip->played = 0;

	SDL_LockAudio();
	list_append(&sfx_clips_to_play, (list_elm_t *)audio_clip);
	SDL_UnlockAudio();

	SDL_PauseAudio(0);
}
