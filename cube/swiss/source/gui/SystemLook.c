/* -----------------------------------------------------------
      SystemLook.c - see SystemLook.h
   ----------------------------------------------------------- */

#include <math.h>
#include <gccore.h>
#include "swiss.h"
#include "SystemLook.h"

static inline GXColor rgba(u8 r, u8 g, u8 b, u8 a) { return (GXColor){r, g, b, a}; }
static inline GXColor withAlpha(GXColor c, float k) {
	if(k < 0) k = 0;
	if(k > 1) k = 1;
	c.a = (u8)(c.a * k);
	return c;
}
static GXColor mix(GXColor a, GXColor b, float t) {
	if(t < 0) t = 0;
	if(t > 1) t = 1;
	return rgba(a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t, a.a + (b.a - a.a) * t);
}

/* ------------------------------------------------------------ screen shape */

void sysViewBounds(float *left, float *top, float *right, float *bottom) {
	// 16:9 TVs stretch the picture by a third, so a third more layout fits across
	float w = swissSettings.uiWidescreen ? 640.0f * 4.0f / 3.0f : 640.0f;
	float h = 480.0f;
	int o = swissSettings.uiOverscan;
	if(o < 0) o = 0;
	if(o > SYS_OVERSCAN_MAX) o = SYS_OVERSCAN_MAX;
	float k = 1.0f - o / 100.0f;	// overscan: the layout shrinks towards the centre
	w /= k;
	h /= k;
	*left = 320.0f - w * 0.5f;
	*right = 320.0f + w * 0.5f;
	*top = 240.0f - h * 0.5f;
	*bottom = 240.0f + h * 0.5f;
}

void sysLoadProjection(void) {
	float l, t, r, b;
	Mtx44 proj;
	sysViewBounds(&l, &t, &r, &b);
	guOrtho(proj, t, b, l, r, 0, 1);
	GX_LoadProjectionMtx(proj, GX_ORTHOGRAPHIC);
}

// One screen pixel, in layout units (lines thinner than this can vanish)
static float pixelW(void) {
	float l, t, r, b;
	sysViewBounds(&l, &t, &r, &b);
	GXRModeObj *m = getVideoMode();
	return (r - l) / (m && m->fbWidth ? m->fbWidth : 640);
}
static float pixelH(void) {
	float l, t, r, b;
	sysViewBounds(&l, &t, &r, &b);
	GXRModeObj *m = getVideoMode();
	return (b - t) / (m && m->efbHeight ? m->efbHeight : 480);
}

/* ------------------------------------------------------------- drawing */

void sysBegin(void) {
	Mtx identity;
	guMtxIdentity(identity);
	GX_LoadPosMtxImm(identity, GX_PNMTX0);	// text drawing leaves its own transform here
	GX_SetNumIndStages(0);
	GX_SetTevDirect(GX_TEVSTAGE0);
	GX_SetNumTevStages(1);
	GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORDNULL, GX_TEXMAP_NULL, GX_COLOR0A0);
	GX_SetTevOp(GX_TEVSTAGE0, GX_PASSCLR);
	GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);
}

// Same vertex layout as the rest of Swiss's drawing (position, colour, texture coordinate)
static inline void vtx(float x, float y, GXColor c) {
	GX_Position3f32(x, y, 0.0f);
	GX_Color4u8(c.r, c.g, c.b, c.a);
	GX_TexCoord2f32(0.0f, 0.0f);
}

void sysRectV(float x, float y, float w, float h, GXColor top, GXColor bottom) {
	if(!(w > 0) || !(h > 0)) return;
	GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
		vtx(x, y, top);
		vtx(x + w, y, top);
		vtx(x + w, y + h, bottom);
		vtx(x, y + h, bottom);
	GX_End();
}

void sysRectH(float x, float y, float w, float h, GXColor left, GXColor right) {
	if(!(w > 0) || !(h > 0)) return;
	GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
		vtx(x, y, left);
		vtx(x + w, y, right);
		vtx(x + w, y + h, right);
		vtx(x, y + h, left);
	GX_End();
}

void sysRect(float x, float y, float w, float h, GXColor c) { sysRectV(x, y, w, h, c, c); }

void sysTriangle(float x0, float y0, float x1, float y1, float x2, float y2, GXColor c) {
	GX_Begin(GX_TRIANGLES, GX_VTXFMT0, 3);
		vtx(x0, y0, c);
		vtx(x1, y1, c);
		vtx(x2, y2, c);
	GX_End();
}

#define RR_SEG 4
#define RR_PTS (4 * (RR_SEG + 1))

static int roundedPoints(float x, float y, float w, float h, float r, float *px, float *py) {
	if(r > w * 0.5f) r = w * 0.5f;
	if(r > h * 0.5f) r = h * 0.5f;
	if(r < 0) r = 0;
	const float cx[4] = { x + w - r, x + w - r, x + r, x + r };
	const float cy[4] = { y + r, y + h - r, y + h - r, y + r };
	int n = 0;
	for(int c = 0; c < 4; c++) {
		float a0 = (-90.0f + 90.0f * c) * (float)M_PI / 180.0f;
		for(int i = 0; i <= RR_SEG; i++) {
			float a = a0 + (float)M_PI * 0.5f * i / RR_SEG;
			px[n] = cx[c] + cosf(a) * r;
			py[n] = cy[c] + sinf(a) * r;
			n++;
		}
	}
	return n;
}

void sysRRect(float x, float y, float w, float h, float r, GXColor top, GXColor bottom) {
	if(!(w > 0) || !(h > 0)) return;
	float px[RR_PTS], py[RR_PTS];
	int n = roundedPoints(x, y, w, h, r, px, py);
	GX_Begin(GX_TRIANGLEFAN, GX_VTXFMT0, n + 2);
		vtx(x + w * 0.5f, y + h * 0.5f, mix(top, bottom, 0.5f));
		for(int i = 0; i <= n; i++) {
			int k = i % n;
			vtx(px[k], py[k], mix(top, bottom, (py[k] - y) / h));
		}
	GX_End();
}

void sysOutline(float x, float y, float w, float h, float r, float t, GXColor c) {
	if(!(w > 0) || !(h > 0)) return;
	// never thinner than a pixel (a hair more, so a line between two pixel centres still lands on one)
	float ux = pixelW() * 1.02f, uy = pixelH() * 1.02f;
	float tx = t < ux ? ux : t, ty = t < uy ? uy : t;
	if(2 * tx >= w || 2 * ty >= h) { sysRRect(x, y, w, h, r, c, c); return; }
	float ox[RR_PTS], oy[RR_PTS], ix[RR_PTS], iy[RR_PTS];
	int n = roundedPoints(x, y, w, h, r, ox, oy);
	float ri = r - (tx > ty ? tx : ty);
	roundedPoints(x + tx, y + ty, w - 2 * tx, h - 2 * ty, ri > 0 ? ri : 0, ix, iy);
	GX_Begin(GX_TRIANGLESTRIP, GX_VTXFMT0, (n + 1) * 2);
		for(int i = 0; i <= n; i++) {
			int k = i % n;
			vtx(ox[k], oy[k], c);
			vtx(ix[k], iy[k], c);
		}
	GX_End();
}

static void shadow(float x, float y, float w, float h, float r, float spread, GXColor c) {
	for(int i = 4; i >= 1; i--) {
		float s = spread * i / 4.0f;
		GXColor k = withAlpha(c, 0.28f);
		sysRRect(x - s, y - s, w + 2 * s, h + 2 * s, r + s, k, k);
	}
}

/* ------------------------------------------------------- pieces of the look */

/* The backdrop: CubeVerse's "soft phosphor" TV screen. A deep blue-black,
 * a cool glow in the middle, soft scanlines bowed by the curve of the glass,
 * and a dark edge shaped like a picture tube. It fills the whole screen
 * whatever its shape (stretching sideways in 16:9, as CubeVerse does). */

static void mesh(int cols, int rows, const float *xy, const GXColor *c) {
	for(int r = 0; r < rows; r++) {
		GX_Begin(GX_TRIANGLESTRIP, GX_VTXFMT0, (cols + 1) * 2);
			for(int k = 0; k <= cols; k++) {
				for(int d = 0; d < 2; d++) {
					int i = (r + d) * (cols + 1) + k;
					vtx(xy[i * 2], xy[i * 2 + 1], c[i]);
				}
			}
		GX_End();
	}
}

static inline float clamp01(float t) { return t < 0 ? 0 : t > 1 ? 1 : t; }

// A point of the screen bowed out like the face of a picture tube (u, v from -1 to 1)
static void tubePoint(float fx, float fy, float fw, float fh, float u, float v, float k, float *x, float *y) {
	float bu = u * (1 + k * v * v), bv = v * (1 + k * 1.4f * u * u);
	*x = fx + fw * (bu + 1) / 2;
	*y = fy + fh * (bv + 1) / 2;
}

// Scanlines: a soft band per line (clear, full, full, then clear at the next line)
static void scanlines(float fx, float fy, float fw, float fh, float period, float bulge, GXColor c) {
	enum { SC = 16, ROWS = 63 };
	static float xy[(SC + 1) * (ROWS + 1) * 2];
	static GXColor col[(SC + 1) * (ROWS + 1)];
	static const float pos[3] = { 0, 0.3f, 0.72f };
	int nrows = (int)(fh / period) * 3 + 1;
	for(int r0 = 0; r0 < nrows - 1; r0 += ROWS) {
		int rows = nrows - 1 - r0 < ROWS ? nrows - 1 - r0 : ROWS;
		for(int j = 0; j <= rows; j++) {
			int r = r0 + j, line = r / 3, ph = r % 3;
			float v = (line + pos[ph]) * period / fh * 2 - 1;
			for(int i = 0; i <= SC; i++) {
				int k = j * (SC + 1) + i;
				tubePoint(fx, fy, fw, fh, (float)i / SC * 2 - 1, v, bulge, &xy[k * 2], &xy[k * 2 + 1]);
				col[k] = rgba(c.r, c.g, c.b, ph ? c.a : 0);
			}
		}
		mesh(SC, rows, xy, col);
	}
}

// The dark edge, shaped like a TV tube: a rounded rectangle (a superellipse)
static void tubeEdge(float fx, float fy, float fw, float fh, float start) {
	enum { VC = 24, VR = 18 };
	static float vxy[(VC + 1) * (VR + 1) * 2];
	static GXColor vc[(VC + 1) * (VR + 1)];
	for(int j = 0; j <= VR; j++) {
		for(int i = 0; i <= VC; i++) {
			int k = j * (VC + 1) + i;
			float u = (float)i / VC * 2 - 1, v = (float)j / VR * 2 - 1;
			vxy[k * 2] = fx + fw * i / VC;
			vxy[k * 2 + 1] = fy + fh * j / VR;
			float au = fabsf(u) * 1.02f, av = fabsf(v) * 1.04f;
			float d = powf(powf(au, 5) + powf(av, 5), 0.2f);	// 1 on the rounded rectangle
			float d2 = sqrtf(u * u * 0.8f + v * v);			// and a gentle round falloff inside
			float t = clamp01((d - start) / (1.0f - start)), t2 = clamp01((d2 - 0.35f) / 0.9f);
			float a = 1 - (1 - powf(t, 1.6f)) * (1 - 0.55f * t2 * t2);
			vc[k] = rgba(0, 0, 0, (u8)(clamp01(a) * 255));
		}
	}
	mesh(VC, VR, vxy, vc);
}

void sysBackdrop(void) {
	float l, t, r, b;
	sysViewBounds(&l, &t, &r, &b);
	float fx = l, fy = t, fw = r - l, fh = b - t;
	sysRectV(fx, fy, fw, fh, rgba(0x1c, 0x24, 0x31, 255), rgba(0x0c, 0x10, 0x16, 255));
	// a cool glow, strongest in the middle, and a fainter one lower down
	enum { C = 12, R = 8 };
	static float xy[(C + 1) * (R + 1) * 2];
	static GXColor col[(C + 1) * (R + 1)];
	const float glow = 1.7f;
	for(int pass = 0; pass < 2; pass++) {
		for(int j = 0; j <= R; j++) {
			for(int i = 0; i <= C; i++) {
				int k = j * (C + 1) + i;
				float x = fx + fw * i / C, y = fy + fh * j / R;
				xy[k * 2] = x;
				xy[k * 2 + 1] = y;
				float dx = (x - (fx + fw / 2)) / (fw * (pass ? 0.45f : 0.6f));
				float dy = pass ? (y - (fy + fh * 0.5f)) / (fh * 0.5f) : (y - (fy + fh * 0.3f)) / (fh * 0.7f);
				float v = clamp01(1 - sqrtf(dx * dx + dy * dy) / (pass ? 0.66f : 0.62f));
				col[k] = pass ? rgba(60, 110, 175, (u8)(clamp01(v * 0.13f * glow) * 255))
				              : rgba(120, 170, 225, (u8)(clamp01(v * 0.17f * glow) * 255));
			}
		}
		mesh(C, R, xy, col);
	}
	scanlines(fx, fy, fw, fh, 8, 0.06f, rgba(0, 0, 0, 150));
	tubeEdge(fx, fy, fw, fh, 0.62f);
}

// Square frame: a light outer line, a dark inner one, and a teal-black fill
void sysPanel(float x, float y, float w, float h) {
	shadow(x, y + 6, w, h, 6, 8, rgba(0, 0, 0, 120));
	sysOutline(x - 3, y - 3, w + 6, h + 6, 6, 2, rgba(200, 208, 220, 140));
	sysOutline(x - 1, y - 1, w + 2, h + 2, 5, 1, rgba(10, 13, 16, 255));
	sysRRect(x, y, w, h, 4, rgba(26, 46, 50, 240), rgba(14, 22, 26, 247));
	// lightest a little above the middle
	float h1 = h * 0.35f;
	sysRectV(x, y + 3, w, h1, rgba(30, 54, 57, 0), rgba(40, 72, 74, 150));
	sysRectV(x, y + 3 + h1, w, h * 0.4f, rgba(40, 72, 74, 150), rgba(30, 54, 57, 0));
	float e = fminf(14, fminf(w, h) * 0.3f);	// darker towards the sides
	sysRectH(x, y + 3, e, h - 6, rgba(0, 0, 0, 70), rgba(0, 0, 0, 0));
	sysRectH(x + w - e, y + 3, e, h - 6, rgba(0, 0, 0, 0), rgba(0, 0, 0, 70));
	sysRect(x + 4, y + 1, w - 8, 1, rgba(255, 255, 255, 46));	// top light
	sysOutline(x, y, w, h, 4, 1, rgba(255, 255, 255, 30));
}

// A fine grid fading in from the left, like the description bar's but softer
static void gridPatch(float x, float y, float w, float h, float cell, float span, bool fromRight, GXColor c) {
	float px = pixelW(), py = pixelH();
	float cx = fmaxf(2, roundf(cell / px)) * px, cy = fmaxf(2, roundf(cell / py)) * py;
	float sw = w * span, x0 = fromRight ? x + w - sw : x;
	float top = ceilf(y / py) * py;
	for(float gy = top + cy * 0.5f; gy < y + h - py; gy += cy) {
		float yy = roundf(gy / py) * py;
		if(fromRight) sysRectH(x0, yy, sw, py, withAlpha(c, 0), c);
		else sysRectH(x0, yy, sw, py, c, withAlpha(c, 0));
	}
	float edge = fromRight ? floorf((x + w) / px) * px : ceilf(x / px) * px;
	for(int i = 0;; i++) {
		float gx = fromRight ? edge - roundf(cx * 0.5f / px) * px - cx * i : edge + roundf(cx * 0.5f / px) * px + cx * i;
		if(gx < x0 || gx > x0 + sw) break;
		float k = 1 - (fromRight ? (x0 + sw - gx) : (gx - x0)) / sw;
		sysRect(gx, top + py, px, h - 2 * py, withAlpha(c, k * k));
	}
}

// Plain frosted glass (no grid, so it stays calm over the backdrop), mostly opaque
void sysFrostRow(float x, float y, float w, float h, int alpha) {
	float k = alpha / 255.0f;
	sysRectV(x, y, w, h, withAlpha(rgba(76, 94, 114, 205), k), withAlpha(rgba(40, 54, 70, 215), k));
	sysRect(x + 1, y + 1, w - 2, 1, withAlpha(rgba(255, 255, 255, 40), k));	// top light
	sysOutline(x, y, w, h, 2, 1, withAlpha(rgba(215, 225, 240, 153), k));
}

void sysButton(float x, float y, float w, float h, bool gold) {
	sysOutline(x - 2, y - 2, w + 4, h + 4, 5, 1, gold ? rgba(230, 200, 70, 115) : rgba(170, 178, 190, 56));
	sysOutline(x - 1, y - 1, w + 2, h + 2, 4, 1, gold ? rgba(40, 32, 4, 230) : rgba(10, 12, 16, 204));
	float m = h * 0.5f;
	GXColor c0 = gold ? rgba(255, 248, 178, 240) : rgba(235, 240, 248, 107);
	GXColor c1 = gold ? rgba(244, 218, 98, 232) : rgba(170, 180, 195, 56);
	GXColor c2 = gold ? rgba(222, 192, 70, 232) : rgba(120, 130, 145, 77);
	sysRect(x, y, w, h, rgba(8, 10, 14, 130));	// glass needs something dark behind it
	sysRRect(x, y, w, m, 2, c0, c1);
	sysRectV(x, y + m, w, h - m, c1, c2);
	sysOutline(x, y, w, h, 2, 1, gold ? rgba(255, 250, 200, 204) : rgba(235, 240, 250, 140));
	sysRect(x + 2, y + 1, w - 4, 1, gold ? rgba(255, 255, 230, 204) : rgba(255, 255, 255, 128));
}

// Gold band 3 units wide just outside the box, with a dark line outside that
void sysGoldRing(float x, float y, float w, float h, bool glow) {
	if(glow) {
		for(int k = 3; k >= 1; k--)
			sysOutline(x - 5 - k * 2, y - 5 - k * 2, w + 10 + k * 4, h + 10 + k * 4, 4 + k * 2, 2.5f, rgba(240, 210, 80, 40 / k));
		sysOutline(x - 5, y - 5, w + 10, h + 10, 1, 2, rgba(106, 90, 16, 255));
	}
	sysOutline(x - 3, y - 3, w + 6, h + 6, 0, 3, rgba(232, 207, 74, 255));
	sysOutline(x, y, w, h, 0, 1, rgba(106, 90, 16, 200));
}

void sysDescBar(float x, float y, float w, float h) {
	sysOutline(x - 1, y - 1, w + 2, h + 2, 4, 1, rgba(11, 11, 32, 255));
	sysRRect(x, y, w, h, 3, rgba(42, 34, 140, 255), rgba(59, 44, 182, 255));
	gridPatch(x + 1, y + 1, w - 2, h - 2, 7, 0.30f, true, rgba(190, 190, 255, 41));
	sysOutline(x, y, w, h, 3, 1, rgba(175, 175, 255, 153));
}

void sysTrack(float x, float y, float w, float h) {
	sysRect(x, y, w, h, rgba(4, 8, 10, 200));
	sysRect(x, y, w, 1, rgba(0, 0, 0, 220));
	sysOutline(x - 1, y - 1, w + 2, h + 2, 1, 1, rgba(200, 208, 220, 90));
}

void sysGoldFill(float x, float y, float w, float h) {
	sysRectV(x, y, w, h * 0.5f, rgba(255, 244, 150, 255), rgba(244, 218, 98, 255));
	sysRectV(x, y + h * 0.5f, w, h * 0.5f, rgba(244, 218, 98, 255), rgba(200, 168, 52, 255));
	sysRect(x, y, w, 1, rgba(255, 255, 230, 220));
}
