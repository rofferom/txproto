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

#include <libplacebo/log.h>
#include <libplacebo/vulkan.h>
#include <libplacebo/renderer.h>
#include <libplacebo/shaders/icc.h>
#include <libplacebo/utils/libav.h>

#include <libtxproto/log.h>

typedef struct PlaceboCtx {
    SPClass *class; // libplacebo class
    AVBufferRef *dev;

    pl_log log;
    pl_gpu pl_gpu;
    pl_vulkan pl_vk_ctx;
    pl_renderer pl_renderer;
} PlaceboCtx;

int placebo_init(PlaceboCtx *ctx, AVBufferRef *dev, VkPresentModeKHR present_mode);
void placebo_uninit(PlaceboCtx *ctx);
