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

#include "pl.h"

static void log_cb_pl(void *ctx, enum pl_log_level level, const char *msg)
{
    int sp_level;

    switch (level) {
    case PL_LOG_FATAL:  sp_level = SP_LOG_FATAL;   break;
    case PL_LOG_ERR:    sp_level = SP_LOG_ERROR;   break;
    case PL_LOG_WARN:   sp_level = SP_LOG_WARN;    break;
    case PL_LOG_INFO:   sp_level = SP_LOG_DEBUG;   break; /* Too spammy for info */
    case PL_LOG_DEBUG:  sp_level = SP_LOG_TRACE;   break;
    case PL_LOG_TRACE:
    default: return;
    }

    sp_log(ctx, sp_level, "%s\n", msg);
}

void placebo_uninit(PlaceboCtx *ctx)
{
    pl_renderer_destroy(&ctx->pl_renderer);
    pl_vulkan_destroy(&ctx->pl_vk_ctx);
    pl_log_destroy(&ctx->log);
}

int placebo_init(PlaceboCtx *ctx, AVBufferRef *dev, VkPresentModeKHR present_mode)
{
    ctx->dev = av_buffer_ref(dev);
    if (!ctx->dev)
        return AVERROR(ENOMEM);

    AVHWDeviceContext *ref_data = (AVHWDeviceContext*)ctx->dev->data;
    AVVulkanDeviceContext *hwctx = ref_data->hwctx;

    ctx->log = pl_log_create(PL_API_VER, &(struct pl_log_params) {
        .log_cb    = log_cb_pl,
        .log_priv  = ctx,
        .log_level = PL_LOG_TRACE,
    });

    /* Now that we have it, init the libplacebo contexts */
    struct pl_vulkan_import_params vkparams = { 0 };
    vkparams.instance             = hwctx->inst;
    vkparams.phys_device          = hwctx->phys_dev;
    vkparams.device               = hwctx->act_dev;
    vkparams.extensions           = hwctx->enabled_dev_extensions;
    vkparams.num_extensions       = hwctx->nb_enabled_dev_extensions;
    vkparams.queue_graphics.index = hwctx->queue_family_index;
    vkparams.queue_graphics.count = hwctx->nb_graphics_queues;
    vkparams.queue_compute.index  = hwctx->queue_family_comp_index;
    vkparams.queue_compute.count  = hwctx->nb_comp_queues;
    vkparams.queue_transfer.index = hwctx->queue_family_tx_index;
    vkparams.queue_transfer.count = hwctx->nb_tx_queues;
    vkparams.features             = &hwctx->device_features;

    ctx->pl_vk_ctx = pl_vulkan_import(ctx->log, &vkparams);
    if (!ctx->pl_vk_ctx) {
        sp_log(ctx, SP_LOG_ERROR, "Error creating libplacebo context!\n");
        av_buffer_unref(&ctx->dev);
        return AVERROR_EXTERNAL;
    }

    /* Set the rendering GPU */
    ctx->pl_gpu = ctx->pl_vk_ctx->gpu;

    /* Set the renderer */
    ctx->pl_renderer = pl_renderer_create(ctx->log, ctx->pl_gpu);

    return 0;
}
