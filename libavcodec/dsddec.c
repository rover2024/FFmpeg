/****************************************************************************
** CFI wrapped code from reading C file 'dsddec__cfic_tmp_new__.c'
**
** Created by: Lorelei CFI compiler
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

//
// CFI declarations begin
//
enum LoreLib_Constants {
    LoreLib_CFI_Count = 19,
};

struct LoreLib_HostLibraryContext {
    void *AddressBoundary;

    void (*HrtSetThreadCallback)(void *callback);
    void *HrtPThreadCreate;
    void *HrtPThreadExit;

    void *CFIs[LoreLib_CFI_Count];
};

__attribute__((visibility("default"))) struct LoreLib_HostLibraryContext LoreLib_HostLibCtx;

#define LORELIB_CFI(INDEX, FP)                                                                       \
    ({                                                                                               \
        typedef __typeof__(FP) _LORELIB_CFI_TYPE;                                                    \
        void *_lorelib_cfi_ret = (void *) (FP);                                                      \
        if ((unsigned long) _lorelib_cfi_ret < (unsigned long) LoreLib_HostLibCtx.AddressBoundary) { \
            LoreLib_HostLibCtx.HrtSetThreadCallback(_lorelib_cfi_ret);                               \
            _lorelib_cfi_ret = (void *) LoreLib_HostLibCtx.CFIs[INDEX - 1];                          \
        }                                                                                            \
        (_LORELIB_CFI_TYPE) _lorelib_cfi_ret;                                                        \
    })

// decl: int (struct AVCodecContext *, int (*)(struct AVCodecContext *, void *, int, int), void *, int *, int)
#define LORELIB_CFI_17(FP) LORELIB_CFI(17, FP)

//
// CFI declarations end
//


//
// Original code begin
//
/*
 * Direct Stream Digital (DSD) decoder
 * based on BSD licensed dsd2pcm by Sebastian Gesemann
 * Copyright (c) 2009, 2011 Sebastian Gesemann. All rights reserved.
 * Copyright (c) 2014 Peter Ross
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/**
 * @file
 * Direct Stream Digital (DSD) decoder
 */

#include "libavutil/mem.h"

#include "avcodec.h"
#include "codec_internal.h"
#include "decode.h"
#include "dsd.h"

#define DSD_SILENCE 0x69
#define DSD_SILENCE_REVERSED 0x96
/* 0x69 = 01101001
 * This pattern "on repeat" makes a low energy 352.8 kHz tone
 * and a high energy 1.0584 MHz tone which should be filtered
 * out completely by any playback system --> silence
 */

static av_cold int decode_init(AVCodecContext *avctx)
{
    DSDContext * s;
    int i;
    uint8_t silence;

    if (!avctx->ch_layout.nb_channels)
        return AVERROR_INVALIDDATA;

    ff_init_dsd_data();

    s = av_malloc_array(sizeof(DSDContext), avctx->ch_layout.nb_channels);
    if (!s)
        return AVERROR(ENOMEM);

    silence = avctx->codec_id == AV_CODEC_ID_DSD_LSBF_PLANAR ||
              avctx->codec_id == AV_CODEC_ID_DSD_LSBF ? DSD_SILENCE_REVERSED : DSD_SILENCE;
    for (i = 0; i < avctx->ch_layout.nb_channels; i++) {
        s[i].pos = 0;
        memset(s[i].buf, silence, sizeof(s[i].buf));
    }

    avctx->sample_fmt = AV_SAMPLE_FMT_FLTP;
    avctx->priv_data  = s;
    return 0;
}

typedef struct ThreadData {
    AVFrame *frame;
    const AVPacket *avpkt;
} ThreadData;

static int dsd_channel(AVCodecContext *avctx, void *tdata, int j, int threadnr)
{
    int lsbf = avctx->codec_id == AV_CODEC_ID_DSD_LSBF || avctx->codec_id == AV_CODEC_ID_DSD_LSBF_PLANAR;
    DSDContext *s = avctx->priv_data;
    ThreadData *td = tdata;
    AVFrame *frame = td->frame;
    const AVPacket *avpkt = td->avpkt;
    int src_next, src_stride;
    float *dst = ((float **)frame->extended_data)[j];

    if (avctx->codec_id == AV_CODEC_ID_DSD_LSBF_PLANAR || avctx->codec_id == AV_CODEC_ID_DSD_MSBF_PLANAR) {
        src_next   = frame->nb_samples;
        src_stride = 1;
    } else {
        src_next   = 1;
        src_stride = avctx->ch_layout.nb_channels;
    }

    ff_dsd2pcm_translate(&s[j], frame->nb_samples, lsbf,
                         avpkt->data + j * src_next, src_stride,
                         dst, 1);

    return 0;
}

static int decode_frame(AVCodecContext *avctx, AVFrame *frame,
                        int *got_frame_ptr, AVPacket *avpkt)
{
    ThreadData td;
    int ret;

    frame->nb_samples = avpkt->size / avctx->ch_layout.nb_channels;

    if ((ret = ff_get_buffer(avctx, frame, 0)) < 0)
        return ret;

    td.frame = frame;
    td.avpkt = avpkt;
    LORELIB_CFI_17(avctx->execute2)(avctx, dsd_channel, &td, NULL, avctx->ch_layout.nb_channels);

    *got_frame_ptr = 1;
    return frame->nb_samples * avctx->ch_layout.nb_channels;
}

#define DSD_DECODER(id_, name_, long_name_) \
const FFCodec ff_ ## name_ ## _decoder = { \
    .p.name       = #name_, \
    CODEC_LONG_NAME(long_name_), \
    .p.type       = AVMEDIA_TYPE_AUDIO, \
    .p.id         = AV_CODEC_ID_##id_, \
    .init         = decode_init, \
    FF_CODEC_DECODE_CB(decode_frame), \
    .p.capabilities = AV_CODEC_CAP_DR1 | AV_CODEC_CAP_SLICE_THREADS, \
    CODEC_SAMPLEFMTS(AV_SAMPLE_FMT_FLTP), \
};

DSD_DECODER(DSD_LSBF, dsd_lsbf, "DSD (Direct Stream Digital), least significant bit first")
DSD_DECODER(DSD_MSBF, dsd_msbf, "DSD (Direct Stream Digital), most significant bit first")
DSD_DECODER(DSD_MSBF_PLANAR, dsd_msbf_planar, "DSD (Direct Stream Digital), most significant bit first, planar")
DSD_DECODER(DSD_LSBF_PLANAR, dsd_lsbf_planar, "DSD (Direct Stream Digital), least significant bit first, planar")

//
// Original code end
//


