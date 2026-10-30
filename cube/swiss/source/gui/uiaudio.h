/* -----------------------------------------------------------
      uiaudio.h - background music and menu sounds for Swiss's own
      screens (the System look), both CubeVerse's and both made from
      scratch at start-up (see musicsynth.c and sfxsynth.c).

      Everything is stopped, and the sound hardware reset, before Swiss
      starts a game or a program (DrawShutdown and the ARAM loaders call
      uiAudioShutdown), so nothing carries over into what's launched.
   ----------------------------------------------------------- */

#ifndef UIAUDIO_H
#define UIAUDIO_H

#include <stdbool.h>

void uiAudioInit(void);            // once, when the screens start
void uiAudioUpdate(void);          // once a frame (sounds for button presses, music loudness)
void uiAudioHoldMusic(bool hold);  // quiet the music (while Swiss's MP3 player is open)
void uiAudioShutdown(void);        // before launching anything; safe to call more than once

#endif
