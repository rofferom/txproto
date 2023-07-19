/*
 * This file is part of txproto.
 *
 * txproto is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * txproto is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with txproto; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#pragma once

#include <libavutil/buffer.h>
#include <libavutil/dict.h>

typedef struct TXMainContext TXMainContext;

TXMainContext *tx_new(void);

int tx_init(TXMainContext *ctx);

void tx_free(TXMainContext *ctx);

int tx_set_epoch(TXMainContext *ctx, int64_t value);

int tx_commit(TXMainContext *ctx);

AVBufferRef *tx_create_demuxer(
    TXMainContext *ctx,
    const char *name,
    const char *in_url,
    const char *in_format,
    AVDictionary *start_options,
    AVDictionary *init_opts
);

AVBufferRef *tx_create_decoder(
    TXMainContext *ctx,
    const char *dec_name,
    AVDictionary *init_opts
);

AVBufferRef *tx_create_encoder(
    TXMainContext *ctx,
    const char *enc_name,
    const char *name,
    AVDictionary **options,
    AVDictionary *init_opts
);

AVBufferRef *tx_create_muxer(
    TXMainContext *ctx,
    const char *out_url,
    AVDictionary *init_opts
);

int tx_link(
    TXMainContext *ctx,
    AVBufferRef *src,
    AVBufferRef *dst,
    int src_stream_id
);

int tx_register_event(
    TXMainContext *ctx,
    AVBufferRef *target,
    AVBufferRef *event
);
