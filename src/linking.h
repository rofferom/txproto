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

#include "txproto_main.h"

int generic_link(
    TXMainContext *ctx,
    AVBufferRef *obj1,
    AVBufferRef *obj2,
    int autostart,
    const char *src_pad_name,
    const char *dst_pad_name,
    int src_stream_id,
    const char *src_stream_desc
);

// R2 HACK
int add_commit_fn_to_list(TXMainContext *ctx, ctrl_fn fn,
                          AVBufferRef *fn_ctx);

ctrl_fn get_ctrl_fn(void *ctx);

int add_commit_fn_to_list(TXMainContext *ctx, ctrl_fn fn,
                          AVBufferRef *fn_ctx);
