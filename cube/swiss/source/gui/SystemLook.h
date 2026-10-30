/* -----------------------------------------------------------
      SystemLook.h - the GameCube "System" look for Swiss's own screens
      (a TV-screen backdrop, teal-black panels, frosted rows, glass buttons and
      the gold selection ring), plus the screen shape settings: 4:3 or
      16:9 and overscan.

      Everything is drawn in Swiss's usual 640x480 layout; the projection
      widens the visible area for 16:9 and pulls the layout in from the
      edges for overscan, so no screen has to know about either.
   ----------------------------------------------------------- */

#ifndef SYSTEMLOOK_H
#define SYSTEMLOOK_H

#include <gccore.h>
#include <stdbool.h>

#define SYS_OVERSCAN_MAX 10	/* percent */

// The part of the layout that is on screen (wider than 640 in 16:9, larger with overscan)
void sysViewBounds(float *left, float *top, float *right, float *bottom);
// Loads the projection for the current screen shape (call where guOrtho(0,480,0,640) was)
void sysLoadProjection(void);

// Untextured drawing: call sysBegin() first; afterwards the caller restores its own state
void sysBegin(void);
void sysRect(float x, float y, float w, float h, GXColor c);
void sysRectV(float x, float y, float w, float h, GXColor top, GXColor bottom);
void sysRectH(float x, float y, float w, float h, GXColor left, GXColor right);
void sysRRect(float x, float y, float w, float h, float r, GXColor top, GXColor bottom);
void sysOutline(float x, float y, float w, float h, float r, float t, GXColor c);
void sysTriangle(float x0, float y0, float x1, float y1, float x2, float y2, GXColor c);

// Pieces of the look
void sysBackdrop(void);                                            // CubeVerse's TV screen, filling the whole screen
void sysPanel(float x, float y, float w, float h);                 // a window or box
void sysFrostRow(float x, float y, float w, float h, int alpha);   // a list row (alpha 0-255)
void sysButton(float x, float y, float w, float h, bool gold);     // grey (or gold) glass
void sysGoldRing(float x, float y, float w, float h, bool glow);   // the selection, just outside x,y,w,h
void sysDescBar(float x, float y, float w, float h);               // the indigo description bar
void sysTrack(float x, float y, float w, float h);                 // a dark groove (progress, scroll bar)
void sysGoldFill(float x, float y, float w, float h);              // what fills a track

#endif
