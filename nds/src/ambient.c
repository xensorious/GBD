#include "ambient.h"
#include "assets.h"
#include <string.h>

static int min(int a, int b) { return a < b ? a : b; }
static int max(int a, int b) { return a > b ? a : b; }

void ambient_world(Surface *s, uint8_t *viewport_depth,
                   int cam_x, int cam_y, unsigned frame) {
    if (!((frame / 24u) & 1u)) return;
    for (unsigned i = 0; i < WORLD_ANIMATION_COUNT; ++i) {
        const WorldAnimation *a = &world_animations[i];
        int x = a->x - cam_x, y = a->y - cam_y;
        int left = max(0, -x), top = max(0, -y);
        int right = min(a->w, s->w - x), bottom = min(a->h, s->h - y);
        if (right <= left || bottom <= top) continue;
        int width = right - left;
        unsigned source = a->offset + (unsigned)(top * a->w + left);
        unsigned dest = (unsigned)((y + top) * s->w + x + left);
        for (int row = top; row < bottom; ++row) {
            gfx_copy(s->pixels + dest, world_animation_art + source, width);
            memcpy(viewport_depth + dest, world_animation_depth + source,
                   (size_t)width);
            source += a->w;
            dest += (unsigned)s->w;
        }
    }
}

/* No per-particle state or random number stream: pausing the game freezes the
   scene, and crossing a camera boundary never relocates a particle. */
static int triangle(unsigned value, unsigned mask) {
    unsigned phase = value & mask;
    unsigned middle = (mask + 1u) >> 1;
    return (int)(phase < middle ? phase : mask - phase);
}

static void pixel(Surface *s, int x, int y, uint16_t color) {
    if ((unsigned)x < (unsigned)s->w && (unsigned)y < (unsigned)s->h)
        s->pixels[y * s->w + x] = color;
}

static void glow(Surface *s, int x, int y, uint16_t color, unsigned alpha) {
    if ((unsigned)x >= (unsigned)s->w || (unsigned)y >= (unsigned)s->h) return;
    uint16_t *dest = s->pixels + y * s->w + x;
    unsigned old = *dest, inv = 16u - alpha;
    unsigned r = (((old & 31u) * inv) + ((color & 31u) * alpha)) >> 4;
    unsigned g = ((((old >> 5) & 31u) * inv) + (((color >> 5) & 31u) * alpha)) >> 4;
    unsigned b = ((((old >> 10) & 31u) * inv) + (((color >> 10) & 31u) * alpha)) >> 4;
    *dest = (uint16_t)(0x8000u | r | (g << 5) | (b << 10));
}

static void fireflies(Surface *s, int cam_x, int cam_y, unsigned frame) {
    static const short lights[][2] = {
        {675, 96}, {743, 118}, {858, 102}, {925, 176},
        {689, 222}, {811, 205}, {877, 286}, {726, 338},
        {929, 345}, {788, 382}, {670, 409}, {910, 402},
        {137, 327}, {322, 356}, {118, 483}, {332, 538},
        {599, 465}, {746, 485}
    };
    const uint16_t warm = C(239, 221, 134);
    for (unsigned i = 0; i < sizeof(lights) / sizeof(lights[0]); ++i) {
        unsigned age = (frame + i * 43u) & 127u;
        if (age < 24u || age > 84u) continue;
        int x = lights[i][0] + triangle(frame / 9u + i * 5u, 15u) - 3 - cam_x;
        int y = lights[i][1] + triangle(frame / 13u + i * 7u, 15u) - 3 - cam_y;
        if (x < -1 || x > s->w || y < -1 || y > s->h) continue;
        unsigned brightness = age < 40u ? (age - 24u) / 2u :
                              age > 68u ? (84u - age) / 2u : 8u;
        if (!brightness) continue;
        glow(s, x, y, warm, brightness + 3u);
        glow(s, x + 1, y, warm, brightness >> 1);
        if (brightness > 5u) glow(s, x, y - 1, warm, 2u);
    }
}

/* Two 6 x 8 silhouettes share their three shades. Their narrow pose exposes
   the pale underside as the leaf turns, rather than blinking it on and off. */
static const uint8_t leaves[2][48] = {
    {0,0,0,1,0,0, 0,0,1,2,1,0, 0,1,2,3,2,0, 1,2,3,2,1,0,
     0,2,3,2,1,0, 0,1,2,1,0,0, 0,0,1,0,0,0, 0,1,0,0,0,0},
    {0,0,0,1,0,0, 0,0,1,2,0,0, 0,0,2,3,1,0, 0,1,3,2,0,0,
     0,1,3,2,0,0, 0,0,2,1,0,0, 0,0,1,0,0,0, 0,1,0,0,0,0}
};

static void leaf(Surface *s, int x, int y, unsigned pose, int flip) {
    static const uint16_t colors[] = {
        0, C(66, 105, 59), C(137, 163, 74), C(216, 207, 119)
    };
    if (x < -6 || x >= s->w || y < -8 || y >= s->h) return;
    const uint8_t *shape = leaves[pose & 1u];
    for (int j = 0; j < 8; ++j) for (int i = 0; i < 6; ++i) {
        unsigned c = shape[j * 6 + (flip ? 5 - i : i)];
        if (c) pixel(s, x + i, y + j, colors[c]);
    }
}

static void gusts(Surface *s, int cam_x, int cam_y, unsigned frame) {
    unsigned age = frame % 660u;
    if (age >= 100u) return;
    unsigned cycle = frame / 660u;
    /* Alternate the upland grove and the green village edge. A gust is a short
       event in its own place, not an overlay following the camera. */
    int forest = !(cycle & 1u);
    int origin_x = forest ? 562 : 61;
    int origin_y = forest ? 164 + (int)(cycle & 3u) * 27 : 342;
    for (unsigned i = 0; i < 2; ++i) {
        int x = origin_x + (int)age * 5 - (int)i * 58 - cam_x;
        int y = origin_y + (int)i * 47 + (int)(age / 5u)
              + triangle(age / 3u + i * 9u, 31u) - 7 - cam_y;
        leaf(s, x, y, age / 9u + i, (int)i);
    }
}

static void bird(Surface *s, int x, int y, unsigned pose) {
    const uint16_t shade = C(72, 105, 117);
    if (x < -7 || x >= s->w || y < -5 || y >= s->h) return;
    pixel(s, x + 3, y + 2, shade);
    pixel(s, x + 4, y + 2, shade);
    if (pose & 1u) {
        pixel(s, x + 1, y + 1, shade); pixel(s, x + 2, y + 1, shade);
        pixel(s, x + 5, y + 1, shade); pixel(s, x + 6, y, shade);
    } else {
        pixel(s, x + 1, y + 3, shade); pixel(s, x + 2, y + 3, shade);
        pixel(s, x + 5, y + 3, shade); pixel(s, x + 6, y + 4, shade);
    }
}

static void coastal_birds(Surface *s, int cam_x, int cam_y, unsigned frame) {
    unsigned age = frame % 840u;
    if (age >= 150u) return;
    int x = (int)age * 8 - 80 - cam_x;
    int y = 620 + triangle(age / 4u, 31u) - cam_y;
    bird(s, x, y, age / 7u);
    bird(s, x - 23, y + 13, (age + 3u) / 7u);
}

void ambient_foreground(Surface *s, int cam_x, int cam_y, unsigned frame) {
    fireflies(s, cam_x, cam_y, frame);
    gusts(s, cam_x, cam_y, frame);
    coastal_birds(s, cam_x, cam_y, frame);
}
