/* -----------------------------------------------------------
      uiaudio.c - see uiaudio.h

      The sounds and the music player are CubeVerse's, played through
      libogc2's AESND library (which Swiss's MP3 player uses too).

      Menu sounds follow the controller: a soft bell for moving around,
      a swell for A, a hushed chord for B, and so on. They're made on a
      thread of their own at start-up.

      The music is streamed to two mono voices (left and right) in chunks:
      a thread renders chunks into a queue ahead of time, and each time the
      library has finished with the current chunk it asks (from the audio
      interrupt) for the next one. If the thread ever falls behind, a voice
      gets a chunk of silence, never old or half-written sound. If making
      the music takes too much of the processor, it switches to a lighter mix.
   ----------------------------------------------------------- */

#include <gccore.h>
#include <aesndlib.h>
#include <mp3player.h>
#include <ogc/aram.h>
#include <ogc/lwp_watchdog.h>
#include <ogc/machine/processor.h>
#include <malloc.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "swiss.h"
#include "uiaudio.h"
#include "sfxsynth.h"
#include "musicsynth.h"

#define MUSIC_LEVEL 0.30f	// how loud the music plays (0..1)
#define SFX_VOLUME  200		// how loud the sounds play (0..255)

static volatile bool ready;
static mutex_t lock = LWP_MUTEX_NULL;

static u32 nowMs(void) { return ticks_to_millisecs(gettime()); }
static void sleepMs(u32 ms) { usleep(ms * 1000); }

/* Before each of a voice's copies to sound memory, wait for any copy still
 * in progress: the library copies every voice through one shared buffer. */
static void waitCopies(void) {
	for(int k = 0; k < 20000 && AR_GetDMAStatus(); k++) {}
}

/* ------------------------------------------------------------ menu sounds */

static s16 *sfxBuf[SFX_COUNT];
static u32 sfxSize[SFX_COUNT];
static lwp_t sfxThread = LWP_THREAD_NULL;
static volatile bool sfxQuit;

static void makeSound(int id) {
	int n = sfx_length(id);
	if(n <= 0) return;
	u32 bytes = ((u32)n * 2 + 31) & ~31u;
	s16 *buf = memalign(32, bytes);
	if(!buf) return;
	memset(buf, 0, bytes);
	if(!sfx_render(id, buf, n)) { free(buf); return; }
	DCFlushRange(buf, bytes);
	sfxSize[id] = bytes;
	__sync_synchronize();	// the size is in place before the sound counts as made
	sfxBuf[id] = buf;
}

// Only the sounds Swiss uses
static const Sfx used[] = { SFX_HOVER, SFX_HOVER2, SFX_HOVER3, SFX_SELECT, SFX_BACK, SFX_CONFIRM,
                            SFX_TURN_SIDE, SFX_OPEN, SFX_KEY };

static void *sfxMain(void *arg) {
	for(unsigned k = 0; k < sizeof(used) / sizeof(used[0]) && !sfxQuit; k++) makeSound(used[k]);
	return NULL;
}

static void sfxCallback(AESNDPB *pb, u32 st) {
	if(st == VOICE_STATE_RUNNING) waitCopies();
}

// A few voices of their own; each plays one sound once and stops itself
#define SFX_VOICES 6
static AESNDPB *sfxVoice[SFX_VOICES];
static u64 sfxBusyUntil[SFX_VOICES];
static int sfxNext;

static void playSound(Sfx s) {
	static int moveTurn;	// moving around takes turns between three bells, like the system menu
	if(s == SFX_HOVER) { s = (Sfx)(SFX_HOVER + moveTurn); moveTurn = (moveTurn + 1) % 3; }
	if(!ready || swissSettings.uiSoundsOff || !sfxBuf[s]) return;
	LWP_MutexLock(lock);
	if(ready) {
		u64 now = gettime();
		int v = -1;
		// a voice whose last sound has finished, so starting it can't collide with the mixer
		for(int k = 0; k < SFX_VOICES && v < 0; k++) {
			int i = (sfxNext + k) % SFX_VOICES;
			if(sfxVoice[i] && now >= sfxBusyUntil[i]) v = i;
		}
		if(v >= 0) {
			sfxNext = (v + 1) % SFX_VOICES;
			AESND_SetVoiceVolume(sfxVoice[v], SFX_VOLUME, SFX_VOLUME);
			AESND_PlayVoice(sfxVoice[v], VOICE_MONO16, sfxBuf[s], sfxSize[s], SFX_RATE, 0, false);
			sfxBusyUntil[v] = now + millisecs_to_ticks((sfxSize[s] / 2) * 1000 / SFX_RATE + 30);
		}
	}
	LWP_MutexUnlock(lock);
}

/* ------------------------------------------------------------------ music */

#define CHUNK_FRAMES 1152		// 2304 bytes a channel: two of the library's copies
#define CHUNK_BYTES (CHUNK_FRAMES * 2)
#define NCHUNKS 24			// the queue: about 1.5 seconds

static int16_t chunkL[NCHUNKS][CHUNK_FRAMES] __attribute__((aligned(32)));
static int16_t chunkR[NCHUNKS][CHUNK_FRAMES] __attribute__((aligned(32)));
static int16_t quiet[CHUNK_FRAMES] __attribute__((aligned(32)));
static int16_t stereo[CHUNK_FRAMES * 2];
static volatile uint32_t handedL, handedR;	// chunks given to each voice (in the interrupt)
static volatile uint32_t filled;		// chunks made (by the thread)
static volatile uint32_t runsL, runsR;		// times each voice was mixed
static volatile uint32_t asksL, asksR;		// times each voice asked for a chunk
static volatile float load;			// time spent making music, of real time
static volatile float target;			// wanted loudness (0..1)
static volatile bool holdMusic;
static volatile bool voicesOn;
static AESNDPB *vl, *vr;
static lwp_t musicThread = LWP_THREAD_NULL;
static volatile bool musicQuit;
static u32 startAt;

// Run in the audio interrupt
static void leftCallback(AESNDPB *pb, u32 st) {
	if(st == VOICE_STATE_RUNNING) { runsL++; waitCopies(); }
	else if(st == VOICE_STATE_STREAM) {
		asksL++;
		if(filled > handedL) { AESND_SetVoiceBuffer(pb, chunkL[handedL % NCHUNKS], CHUNK_BYTES); handedL++; }
		else AESND_SetVoiceBuffer(pb, quiet, CHUNK_BYTES);
	}
}
static void rightCallback(AESNDPB *pb, u32 st) {
	if(st == VOICE_STATE_RUNNING) { runsR++; waitCopies(); }
	else if(st == VOICE_STATE_STREAM) {
		asksR++;
		if(handedR < handedL) { AESND_SetVoiceBuffer(pb, chunkR[handedR % NCHUNKS], CHUNK_BYTES); handedR++; }
		else AESND_SetVoiceBuffer(pb, quiet, CHUNK_BYTES);
	}
}

// Makes chunk number `filled` (both channels), and times it
static void makeChunk(float gain) {
	u64 t0 = gettime();
	int k = (int)(filled % NCHUNKS);
	if(gain > 0) {
		ms_render(stereo, CHUNK_FRAMES, gain);
		for(int i = 0; i < CHUNK_FRAMES; i++) { chunkL[k][i] = stereo[i * 2]; chunkR[k][i] = stereo[i * 2 + 1]; }
	} else {
		memset(chunkL[k], 0, CHUNK_BYTES);
		memset(chunkR[k], 0, CHUNK_BYTES);
	}
	DCFlushRange(chunkL[k], CHUNK_BYTES);
	DCFlushRange(chunkR[k], CHUNK_BYTES);
	float used = (float)ticks_to_microsecs(gettime() - t0) / (CHUNK_FRAMES * 1000000.0f / MS_RATE);
	load = load * 0.9f + used * 0.1f;
	// busy (copying files, loading a game): the lighter mix, and back when there's room again
	static int lite;
	if(!lite && load > 0.45f) { lite = 1; ms_set_lite(1); }
	else if(lite && load < 0.20f) { lite = 0; ms_set_lite(0); }
	filled++;
}

/* Stops both voices and makes sure they really have stopped: a change made
 * to a voice while it's being mixed can be undone by the mixer, so ask,
 * watch that it isn't mixed any more, and ask again if it still is. */
static void stopVoices(void) {
	for(int tries = 0; tries < 50; tries++) {
		AESND_SetVoiceStop(vl, true);
		AESND_SetVoiceStop(vr, true);
		sleepMs(12);
		uint32_t a = runsL, b = runsR;
		sleepMs(12);
		if(runsL == a && runsR == b) return;
	}
}

static void startVoices(float *level) {
	handedL = handedR = filled = 0;
	for(int i = 0; i < 4; i++) { *level += (target - *level) * 0.05f; makeChunk(*level); }
	AESND_SetVoiceVolume(vl, 255, 0);
	AESND_SetVoiceVolume(vr, 0, 255);
	// both start in the same 2 ms block, so they stay in step
	u32 isr;
	_CPU_ISR_Disable(isr);
	AESND_PlayVoice(vl, VOICE_MONO16, chunkL[0], CHUNK_BYTES, MS_RATE, 0, true);
	AESND_SetVoiceStream(vl, true);
	AESND_PlayVoice(vr, VOICE_MONO16, chunkR[0], CHUNK_BYTES, MS_RATE, 0, true);
	AESND_SetVoiceStream(vr, true);
	handedL = handedR = 1;
	_CPU_ISR_Restore(isr);
}

static void *musicMain(void *arg) {
	float level = 0;
	bool started = false;
	uint32_t seenL = 0, seenR = 0;
	u32 seenAt = 0;
	while(!musicQuit) {
		if(!started) {
			// wait for the first time the music is wanted; after that the voices keep
			// running for good (playing silence when it's off), never stopped and restarted
			if(target <= 0) { sleepMs(30); continue; }
			ms_reset();
			startVoices(&level);
			started = voicesOn = true;
			seenL = asksL; seenR = asksR; seenAt = nowMs();
		}
		// keep the queue full (the chunk being copied is never touched)
		while(filled < handedR + NCHUNKS - 2 && !musicQuit) {
			float want = target;
			// fade towards the wanted loudness: in over about 2 s, out over 1 s
			float step = CHUNK_FRAMES / (float)MS_RATE / (want > level ? 2.0f : 1.0f) * 0.5f;
			if(level < want) { level += step; if(level > want) level = want; }
			else if(level > want) { level -= step; if(level < want) level = want; }
			makeChunk(level);
		}
		// each voice asks for a chunk about 16 times a second; if either hasn't
		// for half a second, it has been lost: start both again
		u32 t = nowMs();
		if(asksL != seenL && asksR != seenR) { seenL = asksL; seenR = asksR; seenAt = t; }
		else if(t - seenAt > 500) {
			stopVoices();
			startVoices(&level);
			seenL = asksL; seenR = asksR; seenAt = nowMs();
		}
		sleepMs(10);
	}
	if(voicesOn) stopVoices();
	voicesOn = false;
	return NULL;
}

/* -------------------------------------------------------------- the rest */

void uiAudioInit(void) {
	if(ready) return;
	LWP_MutexInit(&lock, false);
	// Swiss's MP3 player starts the sound library; starting it through the player here
	// means opening the player later doesn't start the library a second time
	MP3Player_Init();
	AESND_Pause(false);
	for(int i = 0; i < SFX_VOICES; i++) sfxVoice[i] = AESND_AllocateVoice(sfxCallback);
	vl = AESND_AllocateVoice(leftCallback);
	vr = AESND_AllocateVoice(rightCallback);
	memset(quiet, 0, sizeof(quiet));
	DCFlushRange(quiet, sizeof(quiet));
	ready = true;
	startAt = nowMs() + 1500;	// the music fades in shortly after the screens appear
	sfxQuit = false;
	if(LWP_CreateThread(&sfxThread, sfxMain, NULL, NULL, 32 * 1024, 60) < 0) sfxThread = LWP_THREAD_NULL;
	musicQuit = false;
	if(vl && vr) {
		// above Swiss's main work, below the drawing
		if(LWP_CreateThread(&musicThread, musicMain, NULL, NULL, 32 * 1024, 115) < 0) musicThread = LWP_THREAD_NULL;
	}
}

void uiAudioHoldMusic(bool hold) { holdMusic = hold; }

/* Button presses become sounds. Swiss reads the controllers in many places,
 * so the sounds come from here, once a frame, rather than from each screen. */
void uiAudioUpdate(void) {
	if(!ready) return;
	bool musicOn = !swissSettings.uiMusicOff && !holdMusic && nowMs() >= startAt;
	target = musicOn ? MUSIC_LEVEL : 0;

	static u32 last;
	static int stickFrames;
	u32 held = PAD_ButtonsHeld(PAD_CHAN0) | PAD_ButtonsHeld(PAD_CHAN1) | PAD_ButtonsHeld(PAD_CHAN2) | PAD_ButtonsHeld(PAD_CHAN3);
	u32 down = held & ~last;
	last = held;

	// the stick: a bell when it's pushed, then again every few frames while it's held (as the list scrolls)
	int sy = 0, sx = 0;
	for(int c = 0; c < 4; c++) {
		int y = PAD_StickY(c), x = PAD_StickX(c);
		if(abs(y) > abs(sy)) sy = y;
		if(abs(x) > abs(sx)) sx = x;
	}
	bool stick = abs(sy) >= 16 || abs(sx) >= 16;
	bool stickBell = false;
	if(stick) {
		if(stickFrames == 0 || (stickFrames >= 12 && (stickFrames - 12) % (abs(sy) > 64 ? 5 : 8) == 0)) stickBell = true;
		stickFrames++;
	}
	else stickFrames = 0;

	if(down & PAD_BUTTON_A) playSound(SFX_SELECT);
	else if(down & PAD_BUTTON_B) playSound(SFX_BACK);
	else if(down & PAD_BUTTON_START) playSound(SFX_CONFIRM);
	else if(down & (PAD_TRIGGER_L | PAD_TRIGGER_R)) playSound(SFX_TURN_SIDE);
	else if(down & (PAD_TRIGGER_Z | PAD_BUTTON_Y)) playSound(SFX_OPEN);
	else if(down & PAD_BUTTON_X) playSound(SFX_KEY);
	else if((down & (PAD_BUTTON_UP | PAD_BUTTON_DOWN | PAD_BUTTON_LEFT | PAD_BUTTON_RIGHT)) || stickBell) playSound(SFX_HOVER);
}

void uiAudioShutdown(void) {
	if(!ready) return;
	LWP_MutexLock(lock);
	ready = false;
	LWP_MutexUnlock(lock);
	if(sfxThread != LWP_THREAD_NULL) { sfxQuit = true; LWP_JoinThread(sfxThread, NULL); sfxThread = LWP_THREAD_NULL; }
	if(musicThread != LWP_THREAD_NULL) { musicQuit = true; LWP_JoinThread(musicThread, NULL); musicThread = LWP_THREAD_NULL; }
	MP3Player_Stop();
	// silence, then hand the sound hardware back as it was found
	AESND_Pause(true);
	AESND_Reset();
}
