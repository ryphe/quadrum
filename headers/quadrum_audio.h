#ifndef QUADRUM_AUDIO_H
#define QUADRUM_AUDIO_H

#include "quadrum_engine.h"

#include <windows.h>
#include <mmsystem.h>
#include <stdlib.h>
#include <string.h>

#define STREAM_BUF_SIZE     512
#define STREAM_NUM_BUFS     4
#define MAX_POLY_VOICES     8

typedef struct {
    float buffer[QUADRUM_MAX_SAMPLES]; /* Statically preallocated 32-bit float voice buffers */
    int   length;
    int   pos;
    int   active;
} ActiveVoice;

typedef struct {
    HWAVEOUT         hWaveOut;
    HANDLE           hEvent;
    HANDLE           hThread;
    CRITICAL_SECTION cs;
    volatile int     running;
    int              initialized;
    int              is_float_format;

    WAVEHDR          headers[STREAM_NUM_BUFS];
    float            float_buffers[STREAM_NUM_BUFS][STREAM_BUF_SIZE];
    short            pcm16_buffers[STREAM_NUM_BUFS][STREAM_BUF_SIZE];

    ActiveVoice      voices[MAX_POLY_VOICES]; /* Static allocation, zero heap churn */
} QuadrumAudio;

static QuadrumAudio g_audio = {0};

/* Real-Time Statically Allocated Floating-Point Mixing Block (SIMD 4-wide) */
static void audio_mix_block_float(float* out_buf, int num_samples) {
    EnterCriticalSection(&g_audio.cs);

    int i = 0;
    int simd_limit = num_samples & ~3;

    /* 1. Fast zero output buffer */
    __m128 zero = _mm_setzero_ps();
    for (i = 0; i < simd_limit; i += 4) {
        _mm_storeu_ps(&out_buf[i], zero);
    }
    for (; i < num_samples; ++i) {
        out_buf[i] = 0.0f;
    }

    /* 2. Vectorized voice accumulation */
    for (int v = 0; v < MAX_POLY_VOICES; ++v) {
        if (!g_audio.voices[v].active) continue;

        float* v_buf = g_audio.voices[v].buffer + g_audio.voices[v].pos;
        int remaining = g_audio.voices[v].length - g_audio.voices[v].pos;
        int count = (remaining < num_samples) ? remaining : num_samples;
        int count_simd = count & ~3;

        int s = 0;
        for (; s < count_simd; s += 4) {
            __m128 dst = _mm_loadu_ps(&out_buf[s]);
            __m128 src = _mm_loadu_ps(&v_buf[s]);
            _mm_storeu_ps(&out_buf[s], _mm_add_ps(dst, src));
        }
        for (; s < count; ++s) {
            out_buf[s] += v_buf[s];
        }

        g_audio.voices[v].pos += count;
        if (g_audio.voices[v].pos >= g_audio.voices[v].length) {
            g_audio.voices[v].active = 0;
            g_audio.voices[v].pos = 0;
        }
    }

    /* 3. SIMD clamp [-1.0f, +1.0f] */
    __m128 one = _mm_set1_ps(1.0f);
    for (i = 0; i < simd_limit; i += 4) {
        __m128 mix = _mm_loadu_ps(&out_buf[i]);
        mix = mm_clamp_sym_ps(mix, one);
        _mm_storeu_ps(&out_buf[i], mix);
    }
    for (; i < num_samples; ++i) {
        if (out_buf[i] > 1.0f) out_buf[i] = 1.0f;
        else if (out_buf[i] < -1.0f) out_buf[i] = -1.0f;
    }

    LeaveCriticalSection(&g_audio.cs);
}

static void audio_mix_block_pcm16(short* out_pcm, int num_samples) {
    float temp_mix[STREAM_BUF_SIZE];
    int count = (num_samples > STREAM_BUF_SIZE) ? STREAM_BUF_SIZE : num_samples;

    audio_mix_block_float(temp_mix, count);

    int i = 0;
    int simd_limit = count & ~7; /* 8 samples (2x __m128) per packed step */
    __m128 scale = _mm_set1_ps(32767.0f);

    for (; i < simd_limit; i += 8) {
        __m128 in_lo = _mm_loadu_ps(&temp_mix[i]);
        __m128 in_hi = _mm_loadu_ps(&temp_mix[i + 4]);

        __m128 scaled_lo = _mm_mul_ps(in_lo, scale);
        __m128 scaled_hi = _mm_mul_ps(in_hi, scale);

        __m128i int_lo = _mm_cvtps_epi32(scaled_lo);
        __m128i int_hi = _mm_cvtps_epi32(scaled_hi);

        __m128i pcm16 = _mm_packs_epi32(int_lo, int_hi);
        _mm_storeu_si128((__m128i*)&out_pcm[i], pcm16);
    }

    /* Remainder tail */
    for (; i < count; ++i) {
        float s = temp_mix[i] * 32767.0f;
        if (s > 32767.0f) s = 32767.0f;
        else if (s < -32768.0f) s = -32768.0f;
        out_pcm[i] = (short)s;
    }
}

static DWORD WINAPI audio_thread_proc(LPVOID param) {
    (void)param;
    while (g_audio.running) {
        WaitForSingleObject(g_audio.hEvent, 50);
        if (!g_audio.running) break;

        for (int i = 0; i < STREAM_NUM_BUFS; i++) {
            if (g_audio.headers[i].dwFlags & WHDR_DONE) {
                waveOutUnprepareHeader(g_audio.hWaveOut, &g_audio.headers[i], sizeof(WAVEHDR));

                if (g_audio.is_float_format) {
                    audio_mix_block_float(g_audio.float_buffers[i], STREAM_BUF_SIZE);
                    g_audio.headers[i].dwBufferLength = STREAM_BUF_SIZE * sizeof(float);
                } else {
                    audio_mix_block_pcm16(g_audio.pcm16_buffers[i], STREAM_BUF_SIZE);
                    g_audio.headers[i].dwBufferLength = STREAM_BUF_SIZE * sizeof(short);
                }

                waveOutPrepareHeader(g_audio.hWaveOut, &g_audio.headers[i], sizeof(WAVEHDR));
                waveOutWrite(g_audio.hWaveOut, &g_audio.headers[i], sizeof(WAVEHDR));
            }
        }
    }
    return 0;
}

static void audio_init(void) {
    if (g_audio.initialized) return;
    memset(&g_audio, 0, sizeof(g_audio));

    InitializeCriticalSection(&g_audio.cs);

    for (int i = 0; i < MAX_POLY_VOICES; i++) {
        g_audio.voices[i].length = 0;
        g_audio.voices[i].pos = 0;
        g_audio.voices[i].active = 0;
    }

    g_audio.hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (!g_audio.hEvent) {
        DeleteCriticalSection(&g_audio.cs);
        return;
    }

    /* Try 32-bit Floating Point audio hardware device first */
    WAVEFORMATEX wf = {0};
    wf.wFormatTag = 0x0003; /* WAVE_FORMAT_IEEE_FLOAT */
    wf.nChannels = 1;
    wf.nSamplesPerSec = QUADRUM_SR;
    wf.wBitsPerSample = 32;
    wf.nBlockAlign = (wf.nChannels * wf.wBitsPerSample) / 8;
    wf.nAvgBytesPerSec = wf.nSamplesPerSec * wf.nBlockAlign;

    MMRESULT res = waveOutOpen(&g_audio.hWaveOut, WAVE_MAPPER, &wf, (DWORD_PTR)g_audio.hEvent, 0, CALLBACK_EVENT);
    if (res == MMSYSERR_NOERROR) {
        g_audio.is_float_format = 1;
    } else {
        /* Fallback to 16-bit PCM if hardware driver is legacy */
        wf.wFormatTag = WAVE_FORMAT_PCM;
        wf.wBitsPerSample = 16;
        wf.nBlockAlign = (wf.nChannels * wf.wBitsPerSample) / 8;
        wf.nAvgBytesPerSec = wf.nSamplesPerSec * wf.nBlockAlign;
        res = waveOutOpen(&g_audio.hWaveOut, WAVE_MAPPER, &wf, (DWORD_PTR)g_audio.hEvent, 0, CALLBACK_EVENT);
        if (res != MMSYSERR_NOERROR) {
            CloseHandle(g_audio.hEvent);
            g_audio.hEvent = NULL;
            DeleteCriticalSection(&g_audio.cs);
            return;
        }
        g_audio.is_float_format = 0;
    }

    g_audio.running = 1;
    /* Do NOT set initialized yet -- only after the thread is confirmed running */

    for (int i = 0; i < STREAM_NUM_BUFS; i++) {
        memset(g_audio.float_buffers[i], 0, sizeof(g_audio.float_buffers[i]));
        memset(g_audio.pcm16_buffers[i], 0, sizeof(g_audio.pcm16_buffers[i]));
        memset(&g_audio.headers[i], 0, sizeof(WAVEHDR));

        if (g_audio.is_float_format) {
            g_audio.headers[i].lpData = (LPSTR)g_audio.float_buffers[i];
            g_audio.headers[i].dwBufferLength = STREAM_BUF_SIZE * sizeof(float);
        } else {
            g_audio.headers[i].lpData = (LPSTR)g_audio.pcm16_buffers[i];
            g_audio.headers[i].dwBufferLength = STREAM_BUF_SIZE * sizeof(short);
        }

        waveOutPrepareHeader(g_audio.hWaveOut, &g_audio.headers[i], sizeof(WAVEHDR));
        waveOutWrite(g_audio.hWaveOut, &g_audio.headers[i], sizeof(WAVEHDR));
    }

    g_audio.hThread = CreateThread(NULL, 0, audio_thread_proc, NULL, 0, NULL);
    if (g_audio.hThread) {
        SetThreadPriority(g_audio.hThread, THREAD_PRIORITY_TIME_CRITICAL);
        g_audio.initialized = 1;   /* Success: mark as fully initialized */
    } else {
        /* Thread creation failed: fully tear down so no half-open resources remain */
        g_audio.running = 0;
        waveOutReset(g_audio.hWaveOut);
        for (int i = 0; i < STREAM_NUM_BUFS; i++) {
            if (g_audio.headers[i].dwFlags & WHDR_PREPARED)
                waveOutUnprepareHeader(g_audio.hWaveOut, &g_audio.headers[i], sizeof(WAVEHDR));
        }
        waveOutClose(g_audio.hWaveOut);
        g_audio.hWaveOut = NULL;
        CloseHandle(g_audio.hEvent);
        g_audio.hEvent = NULL;
        DeleteCriticalSection(&g_audio.cs);
        return;
    }
}

/* Fast Voice Trigger (copies only active count to minimize lock hold time) */
static void audio_play(const float* samples, int count) {
    if (!g_audio.initialized || !samples || count <= 0) return;

    int n = (count > QUADRUM_MAX_SAMPLES) ? QUADRUM_MAX_SAMPLES : count;
    int slot = -1;

    /* Step 1: find a free slot under lock */
    EnterCriticalSection(&g_audio.cs);

    for (int i = 0; i < MAX_POLY_VOICES; i++) {
        if (!g_audio.voices[i].active) {
            slot = i;
            break;
        }
    }
    if (slot < 0) {
        int max_pos = -1;
        for (int i = 0; i < MAX_POLY_VOICES; i++) {
            if (g_audio.voices[i].pos > max_pos) {
                max_pos = g_audio.voices[i].pos;
                slot = i;
            }
        }
    }

    if (slot >= 0) {
        /* Deactivate the slot so the audio thread ignores it during the copy */
        g_audio.voices[slot].active = 0;
    }

    LeaveCriticalSection(&g_audio.cs);

    if (slot < 0) return;

    /* Step 2: copy outside the lock (safe because active == 0) */
    memcpy(g_audio.voices[slot].buffer, samples, n * sizeof(float));

    /* Step 3: re-lock to reactivate with fresh data */
    EnterCriticalSection(&g_audio.cs);
    g_audio.voices[slot].length = n;
    g_audio.voices[slot].pos = 0;
    g_audio.voices[slot].active = 1;
    LeaveCriticalSection(&g_audio.cs);
}

static void audio_shutdown(void) {
    if (!g_audio.initialized) return;

    g_audio.running = 0;
    if (g_audio.hEvent) {
        SetEvent(g_audio.hEvent);
    }
    if (g_audio.hThread) {
        WaitForSingleObject(g_audio.hThread, 500);
        CloseHandle(g_audio.hThread);
        g_audio.hThread = NULL;
    }

    if (g_audio.hWaveOut) {
        waveOutReset(g_audio.hWaveOut);
        for (int i = 0; i < STREAM_NUM_BUFS; i++) {
            if (g_audio.headers[i].dwFlags & WHDR_PREPARED) {
                waveOutUnprepareHeader(g_audio.hWaveOut, &g_audio.headers[i], sizeof(WAVEHDR));
            }
        }
        waveOutClose(g_audio.hWaveOut);
        g_audio.hWaveOut = NULL;
    }

    if (g_audio.hEvent) {
        CloseHandle(g_audio.hEvent);
        g_audio.hEvent = NULL;
    }

    DeleteCriticalSection(&g_audio.cs);
    g_audio.initialized = 0;
}

#endif /* QUADRUM_AUDIO_H */