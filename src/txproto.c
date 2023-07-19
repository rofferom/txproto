#include <libavutil/buffer.h>
#include <libavutil/time.h>

#include "decoding.h"
#include "demuxing.h"
#include "encoding.h"
#include "libavutil/opt.h"
#include "linking.h"
#include "muxing.h"
#include "txproto_main.h"

#include "txproto.h"

TXMainContext *tx_new(void)
{
    TXMainContext *ctx = av_mallocz(sizeof(*ctx));
    return ctx;
}

int tx_init(TXMainContext *ctx)
{
    int err;

    if ((err = sp_log_init(SP_LOG_INFO)) < 0)
        return err;

    if ((err = sp_class_alloc(ctx, "tx", SP_TYPE_NONE, NULL)) < 0) {
        sp_log_uninit();
        av_free(ctx);
        return err;
    }

    //sp_log_set_ctx_lvl_str("global", "trace");

    /* Print timestamps in logs */
    sp_log_print_ts(1);

    ctx->events = sp_bufferlist_new();
    ctx->ext_buf_refs = sp_bufferlist_new();
    ctx->epoch_value = ATOMIC_VAR_INIT(0);
    ctx->source_update_cb_ref = -2; /* R2 HACK */

    return 0;
}

void tx_free(TXMainContext *ctx)
{
    if (!ctx)
        return;

    sp_log_set_status(NULL, SP_STATUS_LOCK | SP_STATUS_NO_CLEAR);

    /* Discard queued events */
    sp_eventlist_dispatch(ctx, ctx->events, SP_EVENT_ON_DISCARD, NULL);

    /* Free lists that may carry contexts around */
    sp_bufferlist_free(&ctx->events);

    /* Free all contexts */
    sp_bufferlist_free(&ctx->ext_buf_refs);

    /* Stop logging */
    sp_log_uninit();

    /* Free any auxiliary data */
    sp_class_free(ctx);
    av_free(ctx);
}

static int epoch_event_cb(AVBufferRef *event, void *callback_ctx, void *_ctx,
                          void *dep_ctx, void *data)
{
    int err = 0;
    TXMainContext *ctx = _ctx;
    EpochEventCtx *epoch_ctx = callback_ctx;

    int64_t val = av_gettime_relative() + epoch_ctx->value;
    atomic_store(&ctx->epoch_value, val);

    return err;
}

static void epoch_event_free(void *callback_ctx, void *_ctx, void *dep_ctx)
{
    EpochEventCtx *epoch_ctx = callback_ctx;
    av_buffer_unref(&epoch_ctx->src_ref);
}

int tx_set_epoch(TXMainContext *ctx, int64_t value)
{
    const SPEventType flags = SP_EVENT_FLAG_ONESHOT | SP_EVENT_ON_COMMIT;

    AVBufferRef *epoch_event = sp_event_create(epoch_event_cb,
                                               epoch_event_free,
                                               sizeof(EpochEventCtx),
                                               NULL,
                                               flags,
                                               ctx,
                                               NULL);

    EpochEventCtx *epoch_ctx = av_buffer_get_opaque(epoch_event);
    epoch_ctx->mode = EP_MODE_OFFSET;
    epoch_ctx->value = value;

    int err = sp_eventlist_add(ctx, ctx->events, epoch_event, 0);
    if (err < 0)
        av_buffer_unref(&epoch_event);

    return 0;
}

int tx_commit(TXMainContext *ctx)
{
    sp_eventlist_dispatch(ctx, ctx->events, SP_EVENT_ON_COMMIT, NULL);

    return 0;
}

AVBufferRef *tx_create_demuxer(
    TXMainContext *ctx,
    const char *name,
    const char *in_url,
    const char *in_format,
    AVDictionary *start_options,
    AVDictionary *init_opts
) {
    int err;
    AVBufferRef *mctx_ref = sp_demuxer_alloc();
    DemuxingContext *mctx = (DemuxingContext *)mctx_ref->data;

    mctx->name = name;
    mctx->in_url = in_url;
    mctx->in_format = in_format;
    mctx->start_options = start_options;

    err = sp_demuxer_init(mctx_ref);
    if (err < 0) {
        sp_log(ctx, SP_LOG_ERROR, "Unable to init demuxer: %s!", av_err2str(err));
        goto err;
    }

    if (init_opts) {
        err = sp_demuxer_ctrl(mctx_ref, SP_EVENT_CTRL_OPTS | SP_EVENT_FLAG_IMMEDIATE, init_opts);
        if (err < 0) {
            sp_log(ctx, SP_LOG_ERROR, "Unable to set options: %s!", av_err2str(err));
            goto err;
        }
    }

    sp_bufferlist_append_noref(ctx->ext_buf_refs, mctx_ref);

    return mctx_ref;

err:
    av_buffer_unref(&mctx_ref);
    return NULL;
}

AVBufferRef *tx_create_decoder(
    TXMainContext *ctx,
    const char *dec_name,
    AVDictionary *init_opts
) {
    int err;
    AVBufferRef *dctx_ref = sp_decoder_alloc();
    DecodingContext *dctx = (DecodingContext *)dctx_ref->data;

    dctx->codec = avcodec_find_decoder_by_name(dec_name);
    if (!dctx->codec) {
        sp_log(ctx, SP_LOG_ERROR,"Decoder \"%s\" not found!", dec_name);
        goto err;
    }

    err = sp_decoder_init(dctx_ref);
    if (err < 0) {
        sp_log(ctx, SP_LOG_ERROR, "Unable to init decoder: %s!", av_err2str(err));
        goto err;
    }

    if (init_opts) {
        err = sp_decoder_ctrl(dctx_ref, SP_EVENT_CTRL_OPTS | SP_EVENT_FLAG_IMMEDIATE, init_opts);
        if (err < 0) {
            sp_log(ctx, SP_LOG_ERROR, "Unable to set options: %s!", av_err2str(err));
            goto err;
        }
    }

    sp_bufferlist_append_noref(ctx->ext_buf_refs, dctx_ref);

    return dctx_ref;

err:
    av_buffer_unref(&dctx_ref);
    return NULL;
}

AVBufferRef *tx_create_encoder(
    TXMainContext *ctx,
    const char *enc_name,
    const char *name,
    AVDictionary **options,
    AVDictionary *init_opts
) {
    int err;
    AVBufferRef *ectx_ref = sp_encoder_alloc();
    EncodingContext *ectx = (EncodingContext *)ectx_ref->data;

    ectx->codec = avcodec_find_encoder_by_name(enc_name);
    if (!ectx->codec) {
        sp_log(ctx, SP_LOG_ERROR, "Encoder \"%s\" not found!", enc_name);
        goto err;
    }

    ectx->name = name;

    err = sp_encoder_init(ectx_ref);
    if (err < 0) {
        sp_log(ctx, SP_LOG_ERROR, "Unable to init encoder: %s!", av_err2str(err));
        goto err;
    }

    err = av_opt_set_dict(ectx->avctx, options);
    assert(err == 0);

    if (init_opts) {
        err = sp_encoder_ctrl(ectx_ref, SP_EVENT_CTRL_OPTS | SP_EVENT_FLAG_IMMEDIATE, init_opts);
        if (err < 0) {
            sp_log(ctx, SP_LOG_ERROR, "Unable to set options: %s!", av_err2str(err));
            goto err;
        }
    }

    sp_bufferlist_append_noref(ctx->ext_buf_refs, ectx_ref);

    return ectx_ref;

err:
    av_buffer_unref(&ectx_ref);
    return NULL;
}

AVBufferRef *tx_create_muxer(
    TXMainContext *ctx,
    const char *out_url,
    AVDictionary *init_opts
) {
    int err;
    AVBufferRef *mctx_ref = sp_muxer_alloc();
    MuxingContext *mctx = (MuxingContext *)mctx_ref->data;

    mctx->out_url = out_url;

    err = sp_muxer_init(mctx_ref);
    if (err < 0) {
        sp_log(ctx, SP_LOG_ERROR, "Unable to init muxer: %s!", av_err2str(err));
        goto err;
    }

    if (init_opts) {
        err = sp_muxer_ctrl(mctx_ref, SP_EVENT_CTRL_OPTS | SP_EVENT_FLAG_IMMEDIATE, init_opts);
        if (err < 0) {
            sp_log(ctx, SP_LOG_ERROR, "Unable to set options: %s!", av_err2str(err));
            goto err;
        }
    }

    sp_bufferlist_append_noref(ctx->ext_buf_refs, mctx_ref);

    return mctx_ref;

err:
    av_buffer_unref(&mctx_ref);
    return NULL;
}

int tx_link(
    TXMainContext *ctx,
    AVBufferRef *src,
    AVBufferRef *dst,
    int src_stream_id
) {
    return generic_link(
        ctx,
        src,
        dst,
        1, // autostart,
        NULL, // src_pad_name,
        NULL, // dst_pad_name,
        src_stream_id,
        NULL // src_stream_desc
    );
}
