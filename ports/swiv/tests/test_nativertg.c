/* Host tests for nativertg. Copyright (c) 2026 AmigaChrome Team */
#include "nativertg.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;

#define CHECK(cond) do { if (!(cond)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

static void set_bit(uint8_t *plane, int stride, int x, int y)
{
    plane[y * stride + (x >> 3)] |= (uint8_t)(0x80 >> (x & 7));
}

static void test_single_playfield(void)
{
    uint8_t p[5][2 * 4] = {{0}};
    nativertg_frame f;
    uint8_t out[16 * 4];
    memset(&f, 0, sizeof f);
    f.width = 16; f.height = 4; f.depth = 5;
    for (int i = 0; i < 5; i++) { f.plane[i] = p[i]; f.stride[i] = 2; }
    set_bit(p[0], 2, 3, 1);                     /* colour 1 */
    set_bit(p[0], 2, 4, 1); set_bit(p[4], 2, 4, 1);   /* colour 17 */
    set_bit(p[1], 2, 15, 3); set_bit(p[2], 2, 15, 3); /* colour 6 */
    nativertg_compose(&f, out);
    CHECK(out[1 * 16 + 3] == 1);
    CHECK(out[1 * 16 + 4] == 17);
    CHECK(out[3 * 16 + 15] == 6);
    CHECK(out[0] == 0);

    /* BPLCON1 delay moves a plane right */
    f.scroll[0] = 2;
    nativertg_compose(&f, out);
    CHECK(out[1 * 16 + 5] == 1);
    CHECK(out[1 * 16 + 3] == 0);
}

static void test_ehb(void)
{
    uint8_t p[6][2] = {{0}};
    nativertg_frame f;
    uint8_t out[16];
    uint32_t rgb[16];
    memset(&f, 0, sizeof f);
    f.width = 16; f.height = 1; f.depth = 6;
    for (int i = 0; i < 6; i++) { f.plane[i] = p[i]; f.stride[i] = 2; }
    set_bit(p[0], 2, 0, 0); set_bit(p[5], 2, 0, 0);   /* 33: half of colour 1 */
    f.colour[1] = 0x0fa4;
    nativertg_compose(&f, out);
    CHECK(out[0] == 33);
    nativertg_to_argb(out, 16, f.colour, rgb);
    CHECK(rgb[0] == 0x7f5522);
}

static void test_dual_playfield(void)
{
    uint8_t p[6][2] = {{0}};
    nativertg_frame f;
    uint8_t out[16];
    memset(&f, 0, sizeof f);
    f.width = 16; f.height = 1; f.depth = 6; f.dual_playfield = 1;
    for (int i = 0; i < 6; i++) { f.plane[i] = p[i]; f.stride[i] = 2; }
    set_bit(p[0], 2, 0, 0);                     /* PF1 value 1 */
    set_bit(p[1], 2, 0, 0); set_bit(p[3], 2, 0, 0);   /* PF2 value 3 */
    set_bit(p[5], 2, 1, 0);                     /* PF2 value 4 alone */
    set_bit(p[4], 2, 2, 0);                     /* PF1 value 4 alone */
    nativertg_compose(&f, out);
    CHECK(out[0] == 1);         /* PF1 in front */
    CHECK(out[1] == 12);        /* PF2 colours from 8 */
    CHECK(out[2] == 4);
    f.pf2_priority = 1;
    nativertg_compose(&f, out);
    CHECK(out[0] == 11);
}

static void test_sprites(void)
{
    uint8_t p[1][4] = {{0}};
    uint16_t s0[2] = { 0x8000, 0x0000 };        /* value 1 at x */
    uint16_t s2[2] = { 0xc000, 0xc000 };        /* value 3 */
    uint16_t s3[2] = { 0x8000, 0x8000 };        /* attached: high bits 3 */
    nativertg_frame f;
    uint8_t out[32];
    memset(&f, 0, sizeof f);
    f.width = 32; f.height = 1; f.depth = 1;
    f.plane[0] = p[0]; f.stride[0] = 4;
    set_bit(p[0], 4, 10, 0);
    f.sprite[0] = (nativertg_sprite){ 10, 0, 1, s0, 0 };
    f.sprite[2] = (nativertg_sprite){ 20, 0, 1, s2, 0 };
    f.sprite[3] = (nativertg_sprite){ 20, 0, 1, s3, 1 };
    f.pf1p = 4; f.pf2p = 4;                     /* every sprite in front */
    nativertg_compose(&f, out);
    CHECK(out[10] == 17);
    CHECK(out[20] == 16 + 15);                  /* 3 | 3 << 2 */
    CHECK(out[21] == 16 + 3);                   /* low bits only */
    f.pf1p = 0;                                 /* playfield in front of all */
    nativertg_compose(&f, out);
    CHECK(out[10] == 1);
    CHECK(out[20] == 31);                       /* no playfield there */
}

static void test_scale2x(void)
{
    /* a one-pixel diagonal: the corners beside it fill in */
    const uint8_t in[9] = { 1, 0, 0,
                            0, 1, 0,
                            0, 0, 1 };
    uint8_t out[36];
    nativertg_scale2x(in, 3, 3, out);
    /* the empty pixel (1, 0) beside the line: its lower-left corner sits
     * where its left (1) and below (1) neighbours meet, so it takes 1 and
     * the step becomes a slope; its other corners stay 0 */
    CHECK(out[1 * 6 + 2] == 1);
    CHECK(out[0 * 6 + 2] == 0);
    CHECK(out[0 * 6 + 3] == 0);
    CHECK(out[1 * 6 + 3] == 0);
    /* the line itself keeps its pixels */
    CHECK(out[2 * 6 + 2] == 1 && out[3 * 6 + 3] == 1);

    /* flat areas and straight edges stay exactly as they were */
    const uint8_t edge[8] = { 1, 1, 2, 2,
                              1, 1, 2, 2 };
    uint8_t big[32], tmp[128], big4[128];
    nativertg_scale2x(edge, 4, 2, big);
    for (int y = 0; y < 4; y++)
        for (int x = 0; x < 8; x++)
            CHECK(big[y * 8 + x] == (x < 4 ? 1 : 2));
    CHECK(nativertg_upscale(edge, 4, 2, 4, tmp, big4) == 0);
    for (int y = 0; y < 8; y++)
        for (int x = 0; x < 16; x++)
            CHECK(big4[y * 16 + x] == (x < 8 ? 1 : 2));
    CHECK(nativertg_upscale(edge, 4, 2, 3, tmp, big4) == -1);
}

static void test_colour(void)
{
    CHECK(nativertg_rgb12to24(0x0fff) == 0xffffff);
    CHECK(nativertg_rgb12to24(0x0000) == 0);
    CHECK(nativertg_rgb12to24(0x0123) == 0x112233);
}

/* The first nativertg_compose (30 Oct 2026), pixel by pixel: the reference
 * the line-at-a-time one (1 Nov 2026) must match exactly. */
/* One bit of a plane at playfield x, after its BPLCON1 delay. */
static int ref_plane_bit(const nativertg_frame *f, int p, int x, int y)
{
    int sx = x - f->scroll[p];
    if (sx < 0 || !f->plane[p])
        return 0;
    return f->plane[p][(size_t)y * (size_t)f->stride[p] + (size_t)(sx >> 3)] >> (7 - (sx & 7)) & 1;
}

/* A sprite's two-bit value at (x, y), 0 where it is clear. */
static int ref_sprite_value(const nativertg_sprite *s, int x, int y)
{
    int dx = x - s->x, dy = y - s->y;
    if (s->height <= 0 || !s->data || dx < 0 || dx > 15 || dy < 0 || dy >= s->height)
        return 0;
    return (s->data[dy * 2] >> (15 - dx) & 1) | (s->data[dy * 2 + 1] >> (15 - dx) & 1) << 1;
}

static void ref_compose(const nativertg_frame *f, uint8_t *out)
{
    int depth = f->depth < 1 ? 1 : f->depth > NATIVERTG_MAX_PLANES ? NATIVERTG_MAX_PLANES : f->depth;

    for (int y = 0; y < f->height; y++) {
        for (int x = 0; x < f->width; x++) {
            int idx = 0, front = 0;     /* front: 1 PF1 shows, 2 PF2 shows */

            if (f->dual_playfield) {
                /* odd planes (1, 3, 5) are playfield 1, colours 0-7; even
                 * planes (2, 4, 6) playfield 2, colours 8-15 */
                int v1 = 0, v2 = 0;
                for (int p = 0; p < depth; p += 2)
                    v1 |= ref_plane_bit(f, p, x, y) << (p / 2);
                for (int p = 1; p < depth; p += 2)
                    v2 |= ref_plane_bit(f, p, x, y) << (p / 2);
                if (v2 && (!v1 || f->pf2_priority)) {
                    idx = 8 + v2;
                    front = 2;
                } else if (v1) {
                    idx = v1;
                    front = 1;
                }
            } else {
                for (int p = 0; p < depth; p++)
                    idx |= ref_plane_bit(f, p, x, y) << p;
                /* single playfield counts as playfield 1 for sprite priority,
                 * as the runtime's Denise does (gfx.c) */
                front = idx ? 1 : 0;
            }

            /* Sprites: the lowest-numbered pair in front; a pair shows over a
             * playfield when its number is below that playfield's priority. */
            for (int n = 0; n < NATIVERTG_MAX_SPRITES; n += 2) {
                const nativertg_sprite *se = &f->sprite[n], *so = &f->sprite[n + 1];
                int c0 = ref_sprite_value(se, x, y), c1 = ref_sprite_value(so, x, y);
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
                idx = col;
                break;
            }
            out[(size_t)y * (size_t)f->width + (size_t)x] = (uint8_t)idx;
        }
    }
}

static void test_same_as_reference(void)
{
    static uint8_t pl[6][48 * 64];
    static uint16_t spr[8][2 * 64];
    static uint8_t a[384 * 64], b[384 * 64];
    int differ = 0;
    srand(7);
    for (int it = 0; it < 2000; it++) {
        nativertg_frame f;
        memset(&f, 0, sizeof f);
        f.width = 8 + rand() % 376; f.height = 1 + rand() % 64; f.depth = 1 + rand() % 6;
        f.dual_playfield = rand() % 2; f.pf2_priority = rand() % 2; f.pf1p = rand() % 5; f.pf2p = rand() % 5;
        for (int p = 0; p < 6; p++) {
            f.stride[p] = 48; f.scroll[p] = rand() % 16; f.plane[p] = rand() % 8 ? pl[p] : NULL;
            for (int i = 0; i < 48 * 64; i++) pl[p][i] = (uint8_t)(rand() % 3 ? rand() : 0);
        }
        for (int n = 0; n < 8; n++) {
            for (int i = 0; i < 128; i++) spr[n][i] = (uint16_t)rand();
            f.sprite[n].x = rand() % 400 - 20; f.sprite[n].y = rand() % 70 - 5;
            f.sprite[n].height = rand() % 3 ? rand() % 30 : 0; f.sprite[n].data = rand() % 6 ? spr[n] : NULL;
            f.sprite[n].attached = rand() % 2;
        }
        ref_compose(&f, a);
        nativertg_compose(&f, b);
        if (memcmp(a, b, (size_t)f.width * (size_t)f.height)) differ++;
    }
    CHECK(differ == 0);
}

int main(void)
{
    test_single_playfield();
    test_ehb();
    test_dual_playfield();
    test_sprites();
    test_scale2x();
    test_colour();
    test_same_as_reference();
    if (failures) {
        fprintf(stderr, "nativertg: %d failed\n", failures);
        return 1;
    }
    puts("nativertg: all tests passed");
    return 0;
}
