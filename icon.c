#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

typedef unsigned char  u8;
typedef unsigned short u16;
typedef unsigned int   u32;
typedef int            s32;

#define SUBSAMPLE 8 /* 8x8 subpixels per destination pixel */

#pragma pack(push, 1)
typedef struct {
    u16 reserved;
    u16 type;       /* 1 = ICO */
    u16 count;      /* Number of icon images */
} ICONHEADER;

typedef struct {
    u8  width;
    u8  height;
    u8  colors;
    u8  reserved;
    u16 planes;
    u16 bitCount;
    u32 size;      /* Size of (BITMAPINFOHEADER + XOR bits + AND mask) */
    u32 offset;    /* File offset to image data */
} ICONDIRENTRY;

typedef struct {
    u32 biSize;
    s32 biWidth;
    s32 biHeight;   /* XOR + AND rows */
    u16 biPlanes;
    u16 biBitCount;
    u32 biCompression;
    u32 biSizeImage;
    s32 biXPelsPerMeter;
    s32 biYPelsPerMeter;
    u32 biClrUsed;
    u32 biClrImportant;
} BITMAPINFOHEADER_T;
#pragma pack(pop)

typedef struct {
    int size;
    u32 data_size;
    u8* data;
} IconImageBlob;

/* Geometry in output pixels, scaled per bundle resolution */
typedef struct {
    double border;   /* cyan ring thickness */
    double inset;    /* gap between icon bounds and circle edge */
    double wave;     /* waveform stroke width */
    double margin_x; /* horizontal waveform inset */
    double amp;      /* waveform peak amplitude */
} Geom;

static Geom geom_for_size(int size) {
    Geom g;
    if (size <= 16) {
        g.border = 1.0;  g.inset = 0.5;  g.wave = 1.15; g.margin_x = 2.5;  g.amp = 3.7;
    } else if (size <= 20) {
        g.border = 1.1;  g.inset = 0.5;  g.wave = 1.25; g.margin_x = 3.0;  g.amp = 4.9;
    } else if (size <= 24) {
        g.border = 1.2;  g.inset = 0.75; g.wave = 1.35; g.margin_x = 3.5;  g.amp = 5.6;
    } else if (size <= 32) {
        g.border = 1.5;  g.inset = 1.0;  g.wave = 1.6;  g.margin_x = 5.0;  g.amp = 8.4;
    } else if (size <= 48) {
        g.border = 2.0;  g.inset = 1.5;  g.wave = 2.2;  g.margin_x = 7.5;  g.amp = 12.8;
    } else if (size <= 64) {
        g.border = 2.5;  g.inset = 2.0;  g.wave = 2.8;  g.margin_x = 10.0; g.amp = 17.2;
    } else if (size <= 128) {
        g.border = 4.0;  g.inset = 3.5;  g.wave = 4.5;  g.margin_x = 20.0; g.amp = 36.5;
    } else { /* 256 */
        g.border = 7.0;  g.inset = 6.0;  g.wave = 8.0;  g.margin_x = 40.0; g.amp = 75.5;
    }
    return g;
}

/* Gamma conversion helpers to eliminate dark halos during antialiasing */
static double srgb_to_linear(int c) {
    double v = c / 255.0;
    return (v <= 0.04045) ? (v / 12.92) : pow((v + 0.055) / 1.055, 2.4);
}

static u8 linear_to_srgb(double v) {
    if (v <= 0.0) return 0;
    if (v >= 1.0) return 255;
    double s = (v <= 0.0031308) ? (v * 12.92) : (1.055 * pow(v, 1.0 / 2.4) - 0.055);
    int c = (int)(s * 255.0 + 0.5);
    return (u8)(c < 0 ? 0 : (c > 255 ? 255 : c));
}

/* quadrum palette (unchanged), pre-converted to linear light */
static void quadrum_palette(double fill_lin[3], double cyan_lin[3]) {
    fill_lin[0] = srgb_to_linear(18);  fill_lin[1] = srgb_to_linear(22);  fill_lin[2] = srgb_to_linear(28);
    cyan_lin[0] = srgb_to_linear(56);  cyan_lin[1] = srgb_to_linear(194); cyan_lin[2] = srgb_to_linear(224);
}

static double clamp01(double v) {
    if (v < 0.0) return 0.0;
    if (v > 1.0) return 1.0;
    return v;
}

/* Analytic box-filter coverage of one subpixel cell whose center lies at
 * signed distance d (output px, positive outside) from a shape edge. */
static double cover(double d) {
    return clamp01(0.5 - d * (double)SUBSAMPLE);
}

/* --------------------------------------------------------------------- */
/* Drum one-shot: full-scale attack at t=0 (near-vertical onset, helped  */
/* by a very short snap), exponential body decay at -5/s, ~2.2 cycles of */
/* skin oscillation. Raw peak magnitude is normalized by the caller.     */
/* --------------------------------------------------------------------- */
static double drum_wave(double t) {
    double env  = exp(-t * 5.0);
    double body = cos(t * M_PI * 4.4);                              /* 2.2 cycles */
    double snap = sin(t * M_PI * 2.0) * exp(-t * 60.0) * 0.18;      /* attack click */
    return body * env + snap;
}

/* Distance from (px,py) to the polyline segment neighborhood around
 * sample index ci. The curve is single-valued in x, so only a local
 * window (wide enough for the stroke radius) needs to be tested. */
static double wave_dist(double px, double py,
                        const double* wx, const double* wy,
                        int npts, int ci, int win) {
    double best = 1e30;
    int j0 = ci - win; if (j0 < 0) j0 = 0;
    int j1 = ci + win; if (j1 > npts - 2) j1 = npts - 2;
    for (int j = j0; j <= j1; j++) {
        double ex = wx[j + 1] - wx[j];
        double ey = wy[j + 1] - wy[j];
        double l2 = ex * ex + ey * ey;
        double u = 0.0;
        if (l2 > 0.0) u = ((px - wx[j]) * ex + (py - wy[j]) * ey) / l2;
        if (u < 0.0) u = 0.0; else if (u > 1.0) u = 1.0;
        double dx = wx[j] + u * ex - px;
        double dy = wy[j] + u * ey - py;
        double d2 = dx * dx + dy * dy;
        if (d2 < best) best = d2;
    }
    return sqrt(best);
}

static IconImageBlob render_icon_size(int size) {
    IconImageBlob blob;
    blob.size = size;
    blob.data_size = 0;
    blob.data = NULL;

    const double inv_ss = 1.0 / (double)SUBSAMPLE;

    Geom g = geom_for_size(size);

    double cx    = size * 0.5;
    double cy    = size * 0.5;
    double R     = size * 0.5 - g.inset;   /* outer circle radius */
    double mid_y = size * 0.5;
    double draw_w = (double)size - 2.0 * g.margin_x;
    double half_w = g.wave * 0.5;

    double fill[3], cyan[3];
    quadrum_palette(fill, cyan);

    /* ---- waveform polyline: drum one-shot, normalized to full scale ---- */
    int npts = (int)(draw_w * 8.0) + 1;
    if (npts < 2) npts = 2;
    double* wx = (double*)malloc((size_t)npts * sizeof(double));
    double* wy = (double*)malloc((size_t)npts * sizeof(double));
    if (!wx || !wy) {
        free(wx);
        free(wy);
        return blob;
    }

    double maxabs = 0.0;
    for (int i = 0; i < npts; i++) {
        double t = (double)i / (double)(npts - 1);
        double v = drum_wave(t);
        wx[i] = g.margin_x + t * draw_w;
        wy[i] = v;
        if (fabs(v) > maxabs) maxabs = fabs(v);
    }
    if (maxabs <= 0.0) maxabs = 1.0;
    for (int i = 0; i < npts; i++)
        wy[i] = mid_y - (wy[i] / maxabs) * g.amp;

    double step = draw_w / (double)(npts - 1);
    int win = (int)(half_w / step) + 2;

    /* ---- ICO image payload ---- */
    int xor_size  = size * size * 4;
    int and_stride = ((size + 31) / 32) * 4;
    int and_size   = and_stride * size;

    blob.data_size = (u32)(sizeof(BITMAPINFOHEADER_T) + (size_t)xor_size + (size_t)and_size);
    blob.data = (u8*)calloc(1, blob.data_size);
    if (!blob.data) {
        free(wx);
        free(wy);
        return blob;
    }

    BITMAPINFOHEADER_T bih;
    memset(&bih, 0, sizeof(bih));
    bih.biSize = (u32)sizeof(BITMAPINFOHEADER_T);
    bih.biWidth = size;
    bih.biHeight = size * 2; /* Height for XOR + AND */
    bih.biPlanes = 1;
    bih.biBitCount = 32;
    bih.biSizeImage = (u32)(xor_size + and_size);
    memcpy(blob.data, &bih, sizeof(bih));

    u8* dest_xor = blob.data + sizeof(BITMAPINFOHEADER_T);
    u8* dest_and = dest_xor + xor_size;

    /* ---- analytic supersampled rasterization (cseq math base) ---- */
    for (int y = 0; y < size; y++) {
        u8* out_row = dest_xor + (size_t)(size - 1 - y) * (size_t)size * 4; /* Bottom-up for Windows DIB */
        u8* and_row = dest_and + (size_t)(size - 1 - y) * (size_t)and_stride;

        for (int x = 0; x < size; x++) {
            double acc_r = 0.0, acc_g = 0.0, acc_b = 0.0, acc_a = 0.0;

            for (int sy = 0; sy < SUBSAMPLE; sy++) {
                double py = (double)y + ((double)sy + 0.5) * inv_ss;
                double dy = py - cy;

                for (int sx = 0; sx < SUBSAMPLE; sx++) {
                    double px = (double)x + ((double)sx + 0.5) * inv_ss;
                    double dx = px - cx;
                    double r  = sqrt(dx * dx + dy * dy);

                    /* Disc + cyan ring, analytic coverage */
                    double cov_out = cover(r - R);
                    if (cov_out <= 0.0)
                        continue; /* the waveform never leaves the disc */

                    double cov_in = cover(r - (R - g.border));
                    double ring = cov_out - cov_in;
                    if (ring < 0.0) ring = 0.0;

                    double pr = ring * cyan[0] + cov_in * fill[0];
                    double pg = ring * cyan[1] + cov_in * fill[1];
                    double pb = ring * cyan[2] + cov_in * fill[2];
                    double pa = cov_out;

                    /* Waveform stroke with round end caps */
                    if (px >= g.margin_x - half_w - 1.0 &&
                        px <= g.margin_x + draw_w + half_w + 1.0 &&
                        fabs(py - mid_y) <= g.amp + half_w + 1.0) {
                        int ci = (int)((px - g.margin_x) / step);
                        if (ci < 0) ci = 0;
                        if (ci > npts - 2) ci = npts - 2;

                        double d = wave_dist(px, py, wx, wy, npts, ci, win);
                        double cw = cover(d - half_w);
                        if (cw > 0.0) {
                            pr = cw * cyan[0] + (1.0 - cw) * pr;
                            pg = cw * cyan[1] + (1.0 - cw) * pg;
                            pb = cw * cyan[2] + (1.0 - cw) * pb;
                            pa = cw + (1.0 - cw) * pa;
                        }
                    }

                    acc_r += pr;
                    acc_g += pg;
                    acc_b += pb;
                    acc_a += pa;
                }
            }

            u8* op = out_row + (size_t)x * 4;
            if (acc_a > 0.0) {
                double inv = 1.0 / acc_a;
                op[0] = linear_to_srgb(acc_b * inv);
                op[1] = linear_to_srgb(acc_g * inv);
                op[2] = linear_to_srgb(acc_r * inv);
                double af = acc_a * (255.0 / (double)(SUBSAMPLE * SUBSAMPLE));
                int a = (int)(af + 0.5);
                if (a > 255) a = 255;
                op[3] = (u8)a;
            } else {
                /* 1-bit AND mask is only set for 100% transparent pixels */
                and_row[x >> 3] |= (u8)(0x80u >> (x & 7));
            }
        }
    }

    free(wx);
    free(wy);
    return blob;
}

int export_app_ico_file(const char* filename) {
    /* Full standard DPI bundle: 16 (title bar), 20/24 (scaled title bars), 32 (standard), 48 (taskbar), 64 (high-dpi), 128/256 (large) */
    const int sizes[] = {16, 20, 24, 32, 48, 64, 128, 256};
    const int num_sizes = (int)(sizeof(sizes) / sizeof(sizes[0]));
    IconImageBlob blobs[sizeof(sizes) / sizeof(sizes[0])];

    for (int i = 0; i < num_sizes; i++) {
        blobs[i] = render_icon_size(sizes[i]);
        if (!blobs[i].data) {
            for (int j = 0; j < i; j++) free(blobs[j].data);
            return 0;
        }
    }

    FILE* f = fopen(filename, "wb");
    if (!f) {
        for (int i = 0; i < num_sizes; i++) free(blobs[i].data);
        return 0;
    }

    ICONHEADER header;
    header.reserved = 0;
    header.type = 1;
    header.count = (u16)num_sizes;
    fwrite(&header, sizeof(header), 1, f);

    u32 current_offset = (u32)(sizeof(ICONHEADER) + sizeof(ICONDIRENTRY) * num_sizes);

    for (int i = 0; i < num_sizes; i++) {
        ICONDIRENTRY entry;
        entry.width  = (blobs[i].size >= 256) ? 0 : (u8)blobs[i].size;
        entry.height = (blobs[i].size >= 256) ? 0 : (u8)blobs[i].size;
        entry.colors = 0;
        entry.reserved = 0;
        entry.planes = 1;
        entry.bitCount = 32;
        entry.size = blobs[i].data_size;
        entry.offset = current_offset;

        fwrite(&entry, sizeof(entry), 1, f);
        current_offset += blobs[i].data_size;
    }

    for (int i = 0; i < num_sizes; i++) {
        fwrite(blobs[i].data, 1, blobs[i].data_size, f);
        free(blobs[i].data);
    }

    fclose(f);
    return 1;
}

int main(int argc, char* argv[]) {
    const char* target = (argc > 1) ? argv[1] : "quadrum.ico";
    if (export_app_ico_file(target)) {
        printf("Successfully generated circular drum-synth icon (analytic AA): %s\n", target);
        return 0;
    }
    fprintf(stderr, "Failed to generate icon file.\n");
    return 1;
}
