/* nativertg: an OCS/ECS chipset picture redrawn for an RTG screen.
 * Copyright (c) 2026 AmigaChrome Team
 */
#include "nativertg.h"

#include <string.h>

/* A sprite's two-bit value at (x, y), 0 where it is clear. */
static int sprite_value(const nativertg_sprite *s, int x, int y)
{
    int dx = x - s->x, dy = y - s->y;
    if (s->height <= 0 || !s->data || dx < 0 || dx > 15 || dy < 0 || dy >= s->height)
        return 0;
    return (s->data[dy * 2] >> (15 - dx) & 1) | (s->data[dy * 2 + 1] >> (15 - dx) & 1) << 1;
}

/* The picture is built a line at a time, eight pixels for each plane byte
 * (a table spreads a byte's bits over eight pixels), and sprites are looked
 * at only on the lines and columns they cover. 1 Nov 2026: the first version
 * read every plane bit and every sprite for every pixel, 0.0021 s a frame on
 * a PC core, far too slow for the 68040; this draws the same picture. */
#define LINE_MAX 1024                   /* widest line kept on the stack; wider ones take the slow path */

static uint32_t spread[256][2];         /* byte -> eight pixels of 0 or 1, first pixel in the lowest byte */
static int spread_ready;

static void spread_init(void)
{
    for (int v = 0; v < 256; v++) {
        uint8_t px[8];
        for (int k = 0; k < 8; k++) px[k] = (uint8_t)(v >> (7 - k) & 1);
        memcpy(&spread[v][0], px, 4);
        memcpy(&spread[v][1], px + 4, 4);
    }
    spread_ready = 1;
}

/* Adds plane p's line y, after its delay, to line[] as bit `shift` of each
 * pixel; pixels left of the delay take nothing, as on the chipset. */
static void add_plane(const nativertg_frame *f, int p, int y, int shift, uint8_t *line)
{
    const uint8_t *row;
    int s = f->scroll[p], w = f->width;
    if (!f->plane[p] || s >= w)
        return;
    if (s < 0) s = 0;                   /* a negative delay read nothing left of x 0 before either */
    row = f->plane[p] + (size_t)y * (size_t)f->stride[p];
    for (int b = 0; 8 * b + s < w; b++) {
        uint8_t v = row[b], *o = line + s + 8 * b;
        uint32_t lo, hi;
        uint8_t px[8];
        if (!v) continue;
        lo = spread[v][0] << shift;
        hi = spread[v][1] << shift;
        memcpy(px, &lo, 4);
        memcpy(px + 4, &hi, 4);
        for (int k = 0; k < 8; k++) o[k] |= px[k];
    }
}

/* The sprites over pixel x of line y (as before: the lowest pair in front;
 * a pair shows over a playfield when its number is below that playfield's
 * priority): the colour register, or idx when none shows. */
static int sprite_pixel(const nativertg_frame *f, int x, int y, int idx, int front)
{
    for (int n = 0; n < NATIVERTG_MAX_SPRITES; n += 2) {
        const nativertg_sprite *se = &f->sprite[n], *so = &f->sprite[n + 1];
        int c0 = sprite_value(se, x, y), c1 = sprite_value(so, x, y);
        int pair = n / 2, col;
        if (so->attached && so->height > 0)
            col = (c0 | c1 << 2) ? 16 + (c0 | c1 << 2) : 0;
        else if (c0)
            col = 16 + pair * 4 + c0;
        else if (c1)
            col = 16 + pair * 4 + c1;
        else
            col = 0;
        if (!col)
            continue;
        if ((front == 1 && pair >= f->pf1p) || (front == 2 && pair >= f->pf2p))
            continue;
        return col;
    }
    return idx;
}

void nativertg_compose(const nativertg_frame *f, uint8_t *out)
{
    int depth = f->depth < 1 ? 1 : f->depth > NATIVERTG_MAX_PLANES ? NATIVERTG_MAX_PLANES : f->depth;
    int w = f->width;
    uint8_t l1[LINE_MAX + 16], l2[LINE_MAX + 16];

    if (!spread_ready)
        spread_init();
    for (int y = 0; y < f->height; y++) {
        uint8_t *o = out + (size_t)y * (size_t)w;
        int sx0 = w, sx1 = 0;

        if (w <= LINE_MAX) {
            memset(l1, 0, (size_t)w + 16);
            if (f->dual_playfield) {
                /* odd planes (1, 3, 5) are playfield 1, colours 0-7; even
                 * planes (2, 4, 6) playfield 2, colours 8-15 */
                memset(l2, 0, (size_t)w + 16);
                for (int p = 0; p < depth; p += 2) add_plane(f, p, y, p / 2, l1);
                for (int p = 1; p < depth; p += 2) add_plane(f, p, y, p / 2, l2);
                for (int x = 0; x < w; x++)
                    o[x] = (uint8_t)(l2[x] && (!l1[x] || f->pf2_priority) ? 8 + l2[x] : l1[x]);
            } else {
                for (int p = 0; p < depth; p++) add_plane(f, p, y, p, l1);
                memcpy(o, l1, (size_t)w);
            }
        } else {
            /* wider than the line buffer: bit by bit */
            for (int x = 0; x < w; x++) {
                int v1 = 0, v2 = 0;
                for (int p = 0; p < depth; p++) {
                    int sx = x - f->scroll[p], bit;
                    if (sx < 0 || !f->plane[p]) continue;
                    bit = f->plane[p][(size_t)y * (size_t)f->stride[p] + (size_t)(sx >> 3)] >> (7 - (sx & 7)) & 1;
                    if (f->dual_playfield) { if (p & 1) v2 |= bit << (p / 2); else v1 |= bit << (p / 2); }
                    else v1 |= bit << p;
                }
                o[x] = (uint8_t)(f->dual_playfield && v2 && (!v1 || f->pf2_priority) ? 8 + v2 : v1);
            }
        }

        /* the columns this line's sprites cover */
        for (int n = 0; n < NATIVERTG_MAX_SPRITES; n++) {
            const nativertg_sprite *sp = &f->sprite[n];
            if (sp->height <= 0 || !sp->data || y < sp->y || y >= sp->y + sp->height)
                continue;
            if (sp->x < sx0) sx0 = sp->x;
            if (sp->x + 16 > sx1) sx1 = sp->x + 16;
        }
        if (sx0 < 0) sx0 = 0;
        if (sx1 > w) sx1 = w;
        for (int x = sx0; x < sx1; x++) {
            int idx = o[x], front;
            /* single playfield counts as playfield 1 for sprite priority,
             * as the runtime's Denise does (gfx.c) */
            if (f->dual_playfield)
                front = idx >= 9 ? 2 : idx ? 1 : 0;   /* 9-15: playfield 2 shows */
            else
                front = idx ? 1 : 0;
            o[x] = (uint8_t)sprite_pixel(f, x, y, idx, front);
        }
    }
}

void nativertg_scale2x(const uint8_t *in, int w, int h, uint8_t *out)
{
    size_t ow = (size_t)w * 2;
    for (int y = 0; y < h; y++) {
        const uint8_t *row = in + (size_t)y * (size_t)w;
        const uint8_t *up = y > 0 ? row - w : row;
        const uint8_t *dn = y < h - 1 ? row + w : row;
        uint8_t *o0 = out + (size_t)y * 2 * ow, *o1 = o0 + ow;
        for (int x = 0; x < w; x++) {
            int l = x > 0 ? x - 1 : x, r = x < w - 1 ? x + 1 : x;
            uint8_t c = row[x], n = up[x], s = dn[x], wv = row[l], e = row[r];
            uint8_t a = c, b = c, d = c, g = c;
            /* a corner turns only where the two neighbours beside it match
             * each other and the opposite pair differ: a true diagonal edge */
            if (n != s && wv != e) {
                if (wv == n) a = wv;
                if (n == e) b = e;
                if (wv == s) d = wv;
                if (s == e) g = e;
            }
            o0[x * 2] = a;
            o0[x * 2 + 1] = b;
            o1[x * 2] = d;
            o1[x * 2 + 1] = g;
        }
    }
}

int nativertg_upscale(const uint8_t *in, int w, int h, int factor,
                      uint8_t *tmp, uint8_t *out)
{
    if (factor == 2) {
        nativertg_scale2x(in, w, h, out);
        return 0;
    }
    if (factor == 4) {
        nativertg_scale2x(in, w, h, tmp);
        nativertg_scale2x(tmp, w * 2, h * 2, out);
        return 0;
    }
    if (factor == 8) {
        nativertg_scale2x(in, w, h, out);
        nativertg_scale2x(out, w * 2, h * 2, tmp);
        nativertg_scale2x(tmp, w * 4, h * 4, out);
        return 0;
    }
    return -1;
}

uint32_t nativertg_rgb12to24(uint16_t c)
{
    uint32_t r = c >> 8 & 15, g = c >> 4 & 15, b = c & 15;
    return (r * 17) << 16 | (g * 17) << 8 | b * 17;
}

void nativertg_to_argb(const uint8_t *in, size_t pixels,
                       const uint16_t colour[NATIVERTG_COLOURS], uint32_t *out)
{
    uint32_t pal[64];
    for (int i = 0; i < NATIVERTG_COLOURS; i++) {
        pal[i] = nativertg_rgb12to24(colour[i]);
        /* extra half-brite: six planes, single playfield, colours 32-63 */
        pal[i + 32] = pal[i] >> 1 & 0x7f7f7f;
    }
    for (size_t i = 0; i < pixels; i++)
        out[i] = pal[in[i] & 63];
}
