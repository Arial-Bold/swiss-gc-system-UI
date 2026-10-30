/* musicsynth.c - see musicsynth.h.
 *
 * The piece is Swiss's own (the System look's menu music): an easy bossa
 * nova in D major at 88 beats a minute, warm and unhurried, for browsing
 * a long list of files. Two bars of intro, then A (eight bars, the tune on
 * the marimba), A again (on the whistle, with little marimba arpeggios),
 * B (eight bars, a dreamier middle that drifts to G minor and back) and
 * A once more (marimba and whistle together), about 90 seconds; then it
 * goes round again from the first A. The sounds:
 *  - a round, warm bass in the bossa pattern: root, then the fifth;
 *  - an electric piano comping in the bossa rhythm;
 *  - a marimba and a soft whistle with the tune;
 *  - a quiet shaker on the eighths, a rim click on the clave, a gentle kick;
 *  - a small room and a little echo.
 * Nothing is sampled; every sound is made here, sample by sample. (The
 * synthesiser is CubeVerse's; the piece is new.) */
#include "musicsynth.h"
#include <math.h>
#include <string.h>
#include <stdbool.h>

#define RATE ((float)MS_RATE)
#define TAU 6.28318531f

/* ------------------------------------------------------------- the score */

/* chords: a root for the bass and four notes for the piano */
enum { DMA9, BM9, EM9, A13, FSM9, B7B9, A9SUS, GMA9, GM69, B13, NCH };
static const struct { signed char root; signed char ep[4]; } chords[NCH] = {
    [DMA9] = { 38, { 57, 61, 64, 66 } },    /* D major 9 (A C# E F#) */
    [BM9] = { 47, { 57, 61, 62, 66 } },     /* B minor 9 */
    [EM9] = { 40, { 55, 59, 62, 66 } },     /* E minor 9 */
    [A13] = { 45, { 55, 59, 61, 66 } },     /* A 13 */
    [FSM9] = { 42, { 57, 61, 64, 68 } },    /* F sharp minor 9 */
    [B7B9] = { 47, { 57, 60, 63, 66 } },    /* B 7 flat 9 */
    [A9SUS] = { 45, { 55, 59, 62, 64 } },   /* A 9 sus */
    [GMA9] = { 43, { 57, 59, 62, 66 } },    /* G major 9 */
    [GM69] = { 43, { 58, 62, 64, 69 } },    /* G minor 6/9 */
    [B13] = { 47, { 57, 63, 66, 68 } },     /* B 13 */
};

/* a bar: the chord for its first half and its second half */
typedef struct { unsigned char c1, c2; } Bar;
static const Bar bars_a[8] = {
    { DMA9, DMA9 }, { BM9, BM9 }, { EM9, EM9 }, { A13, A13 },
    { FSM9, FSM9 }, { B7B9, B7B9 }, { EM9, EM9 }, { A9SUS, A13 },
};
static const Bar bars_b[8] = {
    { GMA9, GMA9 }, { GM69, GM69 }, { FSM9, FSM9 }, { B13, B7B9 },
    { EM9, EM9 }, { A13, A13 }, { DMA9, DMA9 }, { A9SUS, A13 },
};

/* the tunes: per bar, notes of { sixteenth, MIDI note, length in sixteenths } */
typedef struct { signed char at, note, len; } Note;
#define END { -1, 0, 0 }
static const Note tune_a[8][7] = {
    { { 0, 78, 3 }, { 3, 76, 3 }, { 6, 73, 2 }, { 8, 74, 6 }, { 14, 76, 2 }, END },
    { { 0, 78, 4 }, { 4, 81, 2 }, { 6, 78, 2 }, { 8, 76, 8 }, END },
    { { 2, 74, 2 }, { 4, 76, 2 }, { 6, 78, 3 }, { 9, 79, 3 }, { 12, 81, 4 }, END },
    { { 0, 78, 6 }, { 6, 76, 2 }, { 8, 73, 4 }, { 12, 71, 2 }, { 14, 73, 2 }, END },
    { { 0, 73, 3 }, { 3, 76, 3 }, { 6, 81, 2 }, { 8, 80, 4 }, { 12, 76, 4 }, END },
    { { 0, 75, 3 }, { 3, 78, 3 }, { 6, 81, 3 }, { 9, 79, 3 }, { 12, 78, 4 }, END },
    { { 0, 79, 2 }, { 2, 78, 2 }, { 4, 76, 4 }, { 8, 74, 2 }, { 10, 76, 2 }, { 12, 78, 4 }, END },
    { { 0, 76, 6 }, { 6, 74, 2 }, { 8, 73, 8 }, END },
};
static const Note tune_b[8][6] = {
    { { 0, 78, 6 }, { 6, 79, 2 }, { 8, 81, 8 }, END },
    { { 0, 82, 6 }, { 6, 81, 2 }, { 8, 76, 8 }, END },
    { { 0, 81, 6 }, { 6, 78, 2 }, { 8, 76, 8 }, END },
    { { 0, 75, 4 }, { 4, 78, 4 }, { 8, 81, 4 }, { 12, 84, 4 }, END },
    { { 0, 83, 6 }, { 6, 81, 2 }, { 8, 78, 8 }, END },
    { { 0, 79, 4 }, { 4, 78, 4 }, { 8, 76, 4 }, { 12, 73, 4 }, END },
    { { 0, 74, 6 }, { 6, 76, 2 }, { 8, 78, 8 }, END },
    { { 0, 76, 8 }, { 12, 76, 2 }, { 14, 73, 2 }, END },
};

/* the order: 2 intro bars, A, A (whistle), B, A (marimba and whistle) */
enum { SEC_INTRO, SEC_A, SEC_A2, SEC_B, SEC_A3 };
#define INTRO_BARS 2
#define SONG_BARS (INTRO_BARS + 32)
#define LOOP_BAR INTRO_BARS
#define BPM 88.0
#define STEP_SECONDS (60.0 / BPM / 4.0)   /* sixteenth notes */

static int section_of(int bar, int *in) {
    if (bar < INTRO_BARS) { *in = bar; return SEC_INTRO; }
    int b = bar - INTRO_BARS;
    *in = b % 8;
    return SEC_A + b / 8;
}

/* --------------------------------------------------------------- tables */

#define TAB 4096
#define TSHIFT 20     /* 32-bit phase -> table index */
static float sin_t[TAB], noise_t[TAB];
static int tabs_ready;

static void make_tables(void) {
    unsigned r = 987654321u;
    for (int i = 0; i < TAB; i++) {
        r = r * 1664525u + 1013904223u;
        noise_t[i] = (float)(r >> 8) / 8388608.0f - 1.0f;
        sin_t[i] = sinf(TAU * i / TAB);
    }
    tabs_ready = 1;
}

/* Phases are 32-bit fractions of a turn, so they wrap by themselves. */
static inline uint32_t phase_inc(float hz) { return (uint32_t)(hz / RATE * 4294967296.0f); }
static inline float mtof(float m) { return 440.0f * powf(2.0f, (m - 69) / 12.0f); }
static inline float decay_k(float seconds) { return expf(-1.0f / (seconds * RATE)); }

static unsigned rng = 1234567;
static inline float frand(void) {   /* 0..1 */
    rng = rng * 1664525u + 1013904223u;
    return (float)(rng >> 8) / 16777216.0f;
}

/* --------------------------------------------------------------- voices */

enum { V_OFF, V_BASS, V_EP, V_MALLET, V_WHISTLE, V_KICK, V_SNAP, V_SHAKER };
typedef struct {
    int kind;
    uint32_t ph, inc, ph2, inc2;     /* oscillator (and a second one / FM modulator) */
    float a, a_inc;                  /* attack progress 0..1 */
    float lvl, dk, rel_k;            /* level; decay while held; decay once let go */
    int hold;                        /* samples until let go */
    float gain, pl, pr;              /* loudness and pan */
    float idx, idx_k;                /* FM brightness / second partial, and its decay */
    float f1, f2;                    /* filter state (noises) */
} Voice;

#define NV 32
static Voice v[NV];
static int lite;
void ms_set_lite(int on) { lite = on; }

static Voice *grab(void) {
    int limit = lite ? 18 : NV, busy = 0;
    for (int i = 0; i < NV; i++) if (v[i].kind != V_OFF) busy++;
    if (busy < limit)
        for (int i = 0; i < NV; i++) if (v[i].kind == V_OFF) return &v[i];
    /* all busy: take the quietest note that's fading */
    Voice *best = NULL;
    for (int i = 0; i < NV; i++)
        if (v[i].kind != V_BASS && v[i].kind != V_WHISTLE && (!best || v[i].lvl < best->lvl)) best = &v[i];
    return best;
}

static void pan(Voice *x, float p) {   /* -1 left .. +1 right, equal power */
    float a = (p + 1) * 0.25f * 3.14159265f;
    x->pl = cosf(a);
    x->pr = sinf(a);
}

static double step_len;   /* samples per sixteenth */

#ifndef MS_MASK
#define MS_MASK 0xff   /* (tests: which kinds of sound to play, a bit each) */
#endif

static Voice *voice(int kind, float hz, float attack, float decay, float release, float len_steps, float gain, float p) {
    if (!(MS_MASK & (1 << kind))) return NULL;
    Voice *x = grab();
    if (!x) return NULL;
    memset(x, 0, sizeof(*x));
    x->kind = kind;
    x->inc = phase_inc(hz);
    x->a_inc = 1.0f / (attack * RATE);
    x->lvl = 1;
    x->dk = decay > 0 ? decay_k(decay) : 1;
    x->rel_k = decay_k(release);
    x->hold = (int)(len_steps * step_len);
    x->gain = gain;
    pan(x, p);
    return x;
}

static void play_bass(int note, float len, float vel) {
    voice(V_BASS, mtof((float)note), 0.004f, 0.9f, 0.05f, len, 0.11f * vel, 0);
}

static void play_ep(int note, float len, float vel, float p) {
    Voice *x = voice(V_EP, mtof((float)note), 0.006f, 1.1f, 0.3f, len, 0.055f * vel, p);
    if (!x) return;
    x->inc2 = x->inc;                       /* 1:1 FM: a soft electric piano */
    x->idx = 0.9f * vel;
    x->idx_k = decay_k(0.25f);
}

static void play_mallet(int note, float vel, float p) {
    Voice *x = voice(V_MALLET, mtof((float)note), 0.004f, 0.7f, 0.7f, 64, 0.11f * vel, p);
    if (!x) return;
    x->inc2 = phase_inc(mtof((float)note) * 3.93f);   /* the bar's bright overtone */
    x->idx = 0.28f;
    x->idx_k = decay_k(0.05f);
}

static void play_whistle(int note, float len, float vel, float p) {
    Voice *x = voice(V_WHISTLE, mtof((float)note), 0.08f, 0, 0.2f, len, 0.068f * vel, p);
    if (!x) return;
    x->inc2 = phase_inc(5.4f);              /* vibrato */
    x->idx = 0;                             /* the vibrato grows in */
}

static void play_kick(float vel) {
    Voice *x = voice(V_KICK, 120, 0.002f, 0.14f, 0.14f, 64, 0.09f * vel, 0);
    if (x) x->idx = 1;                      /* the pitch drop */
}

static void play_snap(float vel) { voice(V_SNAP, 1900, 0.001f, 0.04f, 0.04f, 64, 0.15f * vel, 0.15f); }
static void play_shaker(float vel, float p) { voice(V_SHAKER, 0, 0.008f, 0.045f, 0.045f, 64, 0.075f * vel, p); }

/* ---------------------------------------------------------------- effects */

typedef struct { float *buf; int len, pos; float lp; } Comb;
typedef struct { float *buf; int len, pos; } Allpass;
static float rv_mem[2][4 * 700 + 2 * 340];
static Comb combs[2][4];
static Allpass aps[2][2];
#define DELAY_LEN ((int)(MS_RATE * 60.0 / BPM * 0.75))   /* a dotted eighth */
static float dly[2][MS_RATE * 6 / 10];   /* room for a dotted eighth at this tempo (0.51 s) */
static int dly_pos;
static float dly_lp[2];

static void fx_init(void) {
    static const int cl[4] = { 797, 911, 1031, 1123 }, al[2] = { 556, 341 };   /* a small room */
    memset(rv_mem, 0, sizeof(rv_mem));
    memset(dly, 0, sizeof(dly));
    dly_pos = 0;
    dly_lp[0] = dly_lp[1] = 0;
    for (int c = 0; c < 2; c++) {
        float *p = rv_mem[c];
        for (int k = 0; k < 4; k++) {
            combs[c][k] = (Comb){ p, (cl[k] + c * 23) * MS_RATE / 44100, 0, 0 };
            p += 700;
        }
        for (int k = 0; k < 2; k++) {
            aps[c][k] = (Allpass){ p, (al[k] + c * 23) * MS_RATE / 44100, 0 };
            p += 340;
        }
    }
}

static inline float comb(Comb *c, float x) {
    float y = c->buf[c->pos];
    c->lp = y * 0.55f + c->lp * 0.45f;
    c->buf[c->pos] = x + c->lp * 0.82f;
    if (++c->pos >= c->len) c->pos = 0;
    return y;
}
static inline float allpass(Allpass *a, float x) {
    float b = a->buf[a->pos];
    a->buf[a->pos] = x + b * 0.5f;
    if (++a->pos >= a->len) a->pos = 0;
    return b - x;
}

/* ------------------------------------------------------------ sequencing */

static long step;             /* the next sixteenth to play */
static double to_step;        /* samples until it */
static float mlp_l, mlp_r;    /* the low-pass over everything */

void ms_reset(void) {
    if (!tabs_ready) make_tables();
    memset(v, 0, sizeof(v));
    fx_init();
    step = 0;
    to_step = 0;
    rng = 1234567;
    step_len = STEP_SECONDS * RATE;
    mlp_l = mlp_r = 0;
}

/* the bass in half a bar, bossa style: the root on the beat, the fifth on
 * the "and" of the next beat (or a step into the next chord when it changes) */
static void bass_half(int root, int next_root, int pos, bool lead_in) {
    switch (pos) {
    case 0: play_bass(root, 5.5f, 1.0f); break;
    case 6:
        if (lead_in && next_root != root) {
            int d = next_root - root;
            play_bass(d > 0 ? next_root - 1 : next_root + 1, 1.6f, 0.65f);   /* a step into the next root */
        } else {
            play_bass(root + 7 - (root + 7 > 50 ? 12 : 0), 1.8f, 0.75f);   /* the fifth (kept low) */
        }
        break;
    }
}

static void play_step(long s) {
    long bar_abs = s / 16;
    int pos = (int)(s % 16);
    int bar = bar_abs < SONG_BARS ? (int)bar_abs : LOOP_BAR + (int)((bar_abs - LOOP_BAR) % (SONG_BARS - LOOP_BAR));
    long pass = bar_abs < SONG_BARS ? 0 : (bar_abs - LOOP_BAR) / (SONG_BARS - LOOP_BAR);
    int in;
    int sec = section_of(bar, &in);
    const Bar *b = sec == SEC_B ? &bars_b[in] : &bars_a[in];   /* the intro has A's first two chords */
    int ch = pos < 8 ? b->c1 : b->c2;
    /* the next half bar's chord, for the bass's lead-in */
    int nbar = bar + 1 >= SONG_BARS ? LOOP_BAR : bar + 1, nin;
    int nsec = section_of(nbar, &nin);
    const Bar *nb = nsec == SEC_B ? &bars_b[nin] : &bars_a[nin];
    int next_root = pos < 8 ? chords[b->c2].root : chords[nb->c1].root;

    /* bass (from the intro's second bar) */
    if (!(sec == SEC_INTRO && in == 0))
        bass_half(chords[ch].root, next_root, pos & 7, true);

    /* electric piano: the bossa comping rhythm, two bars long (held chords in B) */
    static const signed char comp[2][16] = {
        { 1, 0, 0, 2, 0, 0, 2, 0, 0, 0, 2, 0, 1, 0, 0, 0 },
        { 0, 0, 2, 0, 0, 0, 2, 0, 1, 0, 0, 2, 0, 0, 2, 0 },
    };
    int hit = comp[bar & 1][pos];
    if (sec == SEC_INTRO && in == 0) {
        if (pos == 0) for (int k = 0; k < 4; k++) play_ep(chords[ch].ep[k], 14, 0.7f, k * 0.3f - 0.45f);
    } else if (sec == SEC_B) {
        if (pos == 0) for (int k = 0; k < 4; k++) play_ep(chords[ch].ep[k], 9, 0.75f, k * 0.3f - 0.45f);
        else if (pos == 10 || (pos == 8 && b->c1 != b->c2))
            for (int k = 0; k < 4; k++) play_ep(chords[ch].ep[k], 3, 0.45f, k * 0.3f - 0.45f);
    } else if (hit) {
        float vel = hit == 1 ? 0.85f : 0.6f;
        for (int k = 0; k < 4; k++) play_ep(chords[ch].ep[k], hit == 1 ? 3.0f : 1.8f, vel * (k == 3 ? 0.85f : 1), k * 0.3f - 0.45f);
    }

    /* the tune */
    const Note *t = NULL;
    if (sec == SEC_A || sec == SEC_A2 || sec == SEC_A3) t = tune_a[in];
    else if (sec == SEC_B) t = tune_b[in];
    for (int k = 0; t && t[k].at >= 0; k++) {
        if (t[k].at != pos) continue;
        int note = t[k].note;
        if (sec == SEC_A) play_mallet(note, 0.95f, -0.1f);
        else if (sec == SEC_A2) play_whistle(note, t[k].len - 0.3f, 1, 0.1f);
        else if (sec == SEC_B) {
            play_whistle(note, t[k].len - 0.3f, 0.9f, 0.1f);
            if (t[k].len >= 6) play_mallet(note - 12, 0.35f, -0.4f);
        } else {
            play_mallet(note + ((pass & 1) ? 12 : 0), 0.9f, -0.15f);
            play_whistle(note - 12, t[k].len - 0.3f, 0.55f, 0.2f);
        }
    }
    /* little marimba arpeggios under the whistle, up and down the chord */
    if (sec == SEC_A2 && (pos & 1) == 0 && pos != 2 && pos != 10) {
        static const signed char order[8] = { 0, 1, 2, 3, 2, 1, 2, 3 };
        play_mallet(chords[ch].ep[order[pos / 2]] + 12, pos == 0 || pos == 8 ? 0.4f : 0.3f, 0.45f);
    }

    /* percussion */
    bool drums = sec != SEC_INTRO;
    if (!(pos & 1)) play_shaker(((pos & 2) ? 0.8f : 0.5f) * (0.85f + 0.3f * frand()), (pos & 2) ? 0.35f : 0.2f);   /* eighths only */
    if (drums) {
        if (pos == 0) play_kick(0.85f);
        if (pos == 8) play_kick(0.5f);
        /* the rim click on the bossa clave (three, then two) */
        static const signed char clave[2][16] = {
            { 1, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
            { 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0 },
        };
        if (clave[bar & 1][pos]) play_snap(sec == SEC_B ? 0.5f : 0.7f);
        /* a fill into the last A */
        if (sec == SEC_B && in == 7 && (pos == 12 || pos == 14)) play_snap(0.55f + 0.1f * (pos - 12));
    } else if (in == 1 && (pos == 8 || pos == 12)) {
        play_snap(0.5f);   /* a count-in */
    }
}

static inline float soft_clip(float x) {
    if (x > 3) x = 3;
    if (x < -3) x = -3;
    return x * (27 + x * x) / (27 + 9 * x * x);
}

/* One voice's envelope for the next sample. */
static inline float envelope(Voice *x) {
    if (x->a < 1) {
        x->a += x->a_inc;
        if (x->a > 1) x->a = 1;
        return x->a * x->a * (3 - 2 * x->a);
    }
    if (x->hold <= 0) x->lvl *= x->rel_k;
    else {
        x->lvl *= x->dk;
        x->hold--;
    }
    return x->lvl;
}

#define BLOCK 64

void ms_render(int16_t *out, int frames, float gain) {
    if (!tabs_ready) ms_reset();
    float dryL[BLOCK], dryR[BLOCK], send[BLOCK], echo[BLOCK];
    int done = 0;
    while (done < frames) {
        if (to_step <= 0) {
            play_step(step++);
            to_step += step_len;
        }
        int n = frames - done;
        if (n > BLOCK) n = BLOCK;
        if (n > (int)ceil(to_step)) n = (int)ceil(to_step);
        if (n < 1) n = 1;
        memset(dryL, 0, sizeof(float) * n);
        memset(dryR, 0, sizeof(float) * n);
        memset(send, 0, sizeof(float) * n);
        memset(echo, 0, sizeof(float) * n);
        for (int k = 0; k < NV; k++) {
            Voice *x = &v[k];
            if (x->kind == V_OFF) continue;
            float g = x->gain;
            switch (x->kind) {
            case V_BASS:
                for (int i = 0; i < n; i++) {
                    float e = envelope(x) * g;
                    float s = sin_t[x->ph >> TSHIFT] + 0.15f * sin_t[(x->ph << 1) >> TSHIFT] + 0.03f * sin_t[(x->ph * 3) >> TSHIFT];
                    x->ph += x->inc;
                    s *= e;
                    dryL[i] += s;
                    dryR[i] += s;
                    send[i] += s * 0.05f;
                }
                break;
            case V_EP:   /* two-operator FM, 1:1 */
                for (int i = 0; i < n; i++) {
                    float e = envelope(x) * g;
                    float m = sin_t[x->ph2 >> TSHIFT];
                    int32_t pm = (int32_t)(x->idx * m * 683565275.0f);
                    float s = sin_t[(x->ph + (uint32_t)pm) >> TSHIFT] * e;
                    x->ph += x->inc;
                    x->ph2 += x->inc2;
                    x->idx = x->idx * x->idx_k + 0.12f * (1 - x->idx_k);   /* settles to a gentle tone */
                    dryL[i] += s * x->pl;
                    dryR[i] += s * x->pr;
                    send[i] += s * 0.35f;
                }
                break;
            case V_MALLET:   /* a sine bar with a quick bright overtone */
                for (int i = 0; i < n; i++) {
                    float e = envelope(x) * g;
                    float s = (sin_t[x->ph >> TSHIFT] + x->idx * sin_t[x->ph2 >> TSHIFT]) * e;
                    x->ph += x->inc;
                    x->ph2 += x->inc2;
                    x->idx *= x->idx_k;
                    dryL[i] += s * x->pl;
                    dryR[i] += s * x->pr;
                    send[i] += s * 0.4f;
                    echo[i] += s * 0.35f;
                }
                break;
            case V_WHISTLE:
                for (int i = 0; i < n; i++) {
                    float e = envelope(x) * g;
                    if (x->idx < 1) x->idx += 1.0f / (0.35f * RATE);
                    float vib = sin_t[x->ph2 >> TSHIFT] * x->idx * 0.005f;
                    x->ph2 += x->inc2;
                    float s = (sin_t[x->ph >> TSHIFT] + 0.06f * sin_t[(x->ph << 1) >> TSHIFT]) * e;
                    x->ph += (uint32_t)((float)x->inc * (1 + vib));
                    dryL[i] += s * x->pl;
                    dryR[i] += s * x->pr;
                    send[i] += s * 0.5f;
                    echo[i] += s * 0.3f;
                }
                break;
            case V_KICK:
                for (int i = 0; i < n; i++) {
                    float e = envelope(x) * g;
                    x->idx *= 0.9975f;
                    float s = sin_t[x->ph >> TSHIFT] * e;
                    x->ph += (uint32_t)((float)x->inc * (0.33f + 0.67f * x->idx));
                    dryL[i] += s;
                    dryR[i] += s;
                }
                break;
            default: {   /* snap and shaker: filtered noise */
                bool snap = x->kind == V_SNAP;
                const float hp = snap ? 0.35f : 0.75f;
                for (int i = 0; i < n; i++) {
                    float e = envelope(x) * g;
                    x->ph = x->ph * 1664525u + 1013904223u;
                    float nz = noise_t[x->ph >> TSHIFT];
                    x->f1 += (nz - x->f1) * hp;          /* low-passed */
                    float s = snap ? (x->f1 - x->f2) : (nz - x->f1);   /* band (snap) or the highs (shaker) */
                    x->f2 += (x->f1 - x->f2) * 0.12f;
                    s *= e;
                    dryL[i] += s * x->pl;
                    dryR[i] += s * x->pr;
                    send[i] += s * (snap ? 0.5f : 0.15f);
                }
            }
            }
            if (x->a >= 1 && x->lvl < 0.002f) x->kind = V_OFF;
        }
        for (int i = 0; i < n; i++) {
            float dl = dryL[i], dr = dryR[i], sn = send[i];
            /* ping-pong echo of the marimba and whistle */
            float el = dly[0][dly_pos], er = dly[1][dly_pos];
            dly_lp[0] += (er - dly_lp[0]) * 0.4f;
            dly_lp[1] += (el - dly_lp[1]) * 0.4f;
            dly[0][dly_pos] = echo[i] + dly_lp[0] * 0.3f;
            dly[1][dly_pos] = dly_lp[1] * 0.3f;
            if (++dly_pos >= DELAY_LEN) dly_pos = 0;
            dl += el * 0.3f;
            dr += er * 0.3f;
            /* the room */
            float in = sn * 0.27f, wl = 0, wr = 0;
            if (lite) {
                wl = (comb(&combs[0][0], in) + comb(&combs[0][2], in)) * 1.6f;
                wr = (comb(&combs[1][1], in) + comb(&combs[1][3], in)) * 1.6f;
            } else {
                for (int k = 0; k < 4; k++) { wl += comb(&combs[0][k], in); wr += comb(&combs[1][k], in); }
            }
            for (int k = 0; k < 2; k++) { wl = allpass(&aps[0][k], wl); wr = allpass(&aps[1][k], wr); }
            /* a gentle low-pass over everything, to keep it soft */
            mlp_l += (dl + wl * 0.65f - mlp_l) * 0.55f;
            mlp_r += (dr + wr * 0.65f - mlp_r) * 0.55f;
            float L = soft_clip(mlp_l * 1.8f) * gain, R = soft_clip(mlp_r * 1.8f) * gain;
            out[(done + i) * 2] = (int16_t)(L * 30000);
            out[(done + i) * 2 + 1] = (int16_t)(R * 30000);
        }
        done += n;
        to_step -= n;
    }
}
