#include <stdio.h>
#include <assert.h>
#include <limits.h>
#include <pthread.h>

#include <libavutil/buffer.h>
#include <libavutil/dict.h>
#include <libavutil/time.h>

#include <libtxproto/events.h>
#include <libtxproto/txproto.h>
#include <libtxproto/utils.h>

// gcc -Wall -g chain_filters.c -o chain_filters `pkg-config --cflags --libs txproto libavutil`

struct EosEvent {
    pthread_cond_t *cond;
    int eos;
};

static int muxer_eos_cb(AVBufferRef *event_ref, void *callback_ctx, void *ctx,
                 void *dep_ctx, void *data)
{
    struct EosEvent *event = callback_ctx;

    printf("End of stream!\n");
    event->eos = 1;

    int err = pthread_cond_signal(event->cond);
    assert(err == 0);

    return 0;
}

enum FilterType {
    TRANSPOSE,
    NEGATE,
    TRANSPOSE_NEGATE_ONE_FILTER,
    TRANSPOSE_NEGATE_TWO_FILTERS,
};

static const struct {
    enum FilterType type;
    const char *str;
} FILTER_TYPE_LIST[] = {
    { .type = TRANSPOSE,                    "transpose", },
    { .type = NEGATE,                       "negate", },
    { .type = TRANSPOSE_NEGATE_ONE_FILTER,  "transpose_negate_one", },
    { .type = TRANSPOSE_NEGATE_TWO_FILTERS, "transpose_negate_two", },
};

int main(int argc, char *argv[])
{
    enum FilterType filter_type = TRANSPOSE_NEGATE_TWO_FILTERS;
    int err;

    if (argc >= 2) {
        for (size_t i = 0; i < SP_ARRAY_ELEMS(FILTER_TYPE_LIST); i++) {
            if (!strcmp(argv[1], FILTER_TYPE_LIST[i].str)) {
                printf("Found filter type '%s'\n", argv[1]);
                filter_type = FILTER_TYPE_LIST[i].type;
                break;
            }
        }
    }

    TXMainContext *ctx = tx_new();
    err = tx_init(ctx);
    assert(err == 0);

    err = tx_epoch_set(ctx, 0);
    assert(err == 0);

    // Demuxer
    AVBufferRef *demuxer = tx_demuxer_create(
        ctx,
        NULL, // Name
        "test.webm", // in_url
        NULL, // in_format
        NULL, // start_options
        NULL // init_opts
    );

    // Decoder
    AVBufferRef *decoder = tx_decoder_create(
        ctx,
        "vp9", // dec_name
        NULL // init_opts
    );

    err = tx_link(ctx, demuxer, decoder, 0);
    assert(err == 0);

    // Filter
    AVBufferRef *filter;

    switch (filter_type) {
    case TRANSPOSE: {
        AVDictionary *filter_options = NULL;
        err = av_dict_set_int(&filter_options, "dump_graph", 1, 0);
        assert(err == 0);

        filter = tx_filtergraph_create(
            ctx,
            "filter-transpose",
            "transpose=0",
            AV_HWDEVICE_TYPE_NONE,
            filter_options);

        err = tx_link(ctx, decoder, filter, 0);
        assert(err == 0);

        break;
    }

    case NEGATE: {
        AVDictionary *filter_options = NULL;
        err = av_dict_set_int(&filter_options, "dump_graph", 1, 0);
        assert(err == 0);

        filter = tx_filtergraph_create(
            ctx,
            "filter-negate",
            "negate",
            AV_HWDEVICE_TYPE_NONE,
            filter_options);

        err = tx_link(ctx, decoder, filter, 0);
        assert(err == 0);

        break;
    }

    case TRANSPOSE_NEGATE_ONE_FILTER: {
        AVDictionary *filter_options = NULL;
        err = av_dict_set_int(&filter_options, "dump_graph", 1, 0);
        assert(err == 0);

        filter = tx_filtergraph_create(
            ctx,
            "filter-transpose-negate",
            "transpose=0,negate",
            AV_HWDEVICE_TYPE_NONE,
            filter_options);

        err = tx_link(ctx, decoder, filter, 0);
        assert(err == 0);

        break;
    }

    case TRANSPOSE_NEGATE_TWO_FILTERS: {
        AVDictionary *filter_options = NULL;

        // Transpose
        err = av_dict_set_int(&filter_options, "dump_graph", 1, 0);
        assert(err == 0);

        AVBufferRef *transpose = tx_filtergraph_create(
            ctx,
            "filter-transpose",
            "transpose=0",
            AV_HWDEVICE_TYPE_NONE,
            filter_options);

        err = tx_link(ctx, decoder, transpose, 0);
        assert(err == 0);

        // Negate
        filter_options = NULL;
        err = av_dict_set_int(&filter_options, "dump_graph", 1, 0);
        assert(err == 0);

        AVBufferRef *negate = tx_filtergraph_create(
            ctx,
            "filter-negate",
            "negate",
            AV_HWDEVICE_TYPE_NONE,
            filter_options);

        err = tx_link(ctx, transpose, negate, 0);
        assert(err == 0);

        filter = negate;

        break;
    }

    default:
        sp_assert(0);
    }

    // Encoder
    AVDictionary *encoder_options = NULL;
    err = av_dict_set(&encoder_options, "b", "20M", 0);
    assert(err == 0);

    err = av_dict_set(&encoder_options, "bf", "0", 0);
    assert(err == 0);

    AVDictionary *encoder_init_options = NULL;
    err = av_dict_set(&encoder_init_options, "fifo_flags", "block_no_input,block_max_output", 0);
    assert(err == 0);

    AVBufferRef *encoder = tx_encoder_create(
        ctx,
        "libx264",
        NULL, // name
        encoder_options,
        encoder_init_options
    );

    err = tx_link(ctx, filter, encoder, 0);
    assert(err == 0);

    // Muxer
    AVBufferRef *muxer = tx_muxer_create(
        ctx,
        "out.mkv",
        NULL, // out_format
        NULL, // options
        NULL  // init_opts
    );

    err = tx_link(ctx, encoder, muxer, 0);
    assert(err == 0);

    // Setup End of stream handler
    pthread_mutex_t mtx;
    err = pthread_mutex_init(&mtx, NULL);
    assert(err == 0);

    pthread_cond_t cond;
    err = pthread_cond_init(&cond, NULL);
    assert(err == 0);

    AVBufferRef *muxer_eof_event = NULL;
    muxer_eof_event = sp_event_create(muxer_eos_cb,
                                     NULL,
                                     sizeof(struct EosEvent),
                                     NULL,
                                     SP_EVENT_ON_EOS,
                                     NULL,
                                     NULL);

    struct EosEvent *eos_event = av_buffer_get_opaque(muxer_eof_event);
    eos_event->cond = &cond;
    eos_event->eos = 0;

    err = tx_event_register(ctx, muxer, muxer_eof_event);
    assert(err == 0);

    // Commit
    err = tx_commit(ctx);
    assert(err == 0);

    // Wait for EOS
    err = pthread_mutex_lock(&mtx);
    assert(err == 0);

    while (!eos_event->eos) {
        err = pthread_cond_wait(&cond, &mtx);
        assert(err == 0);
    }

    err = pthread_mutex_unlock(&mtx);
    assert(err == 0);

    // Free everything
    av_buffer_unref(&muxer_eof_event);
    pthread_mutex_destroy(&mtx);
    pthread_cond_destroy(&cond);
    tx_free(ctx);

    return 0;
}
