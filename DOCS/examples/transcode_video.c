#include <stdio.h>
#include <assert.h>
#include <limits.h>
#include <pthread.h>

#include <libavutil/buffer.h>
#include <libavutil/time.h>

#include <libtxproto/events.h>
#include <libtxproto/txproto.h>

// gcc -Wall -g transcode_video.c -o transcode_video `pkg-config --cflags --libs txproto libavutil`

int muxer_eos_cb(AVBufferRef *event_ref, void *callback_ctx, void *ctx,
                 void *dep_ctx, void *data)
{
    printf("End of stream!\n");

    return 0;
}

int main(int argc, char *argv[])
{
    int err;

    TXMainContext *ctx = tx_new();
    err = tx_init(ctx);
    assert(err == 0);

    err = tx_set_epoch(ctx, 0);
    assert(err == 0);

    // Demuxer
    AVBufferRef *demuxer = tx_create_demuxer(
        ctx,
        NULL, // Name
        "test.webm", // in_url
        NULL, // in_format
        NULL, // start_options
        NULL // init_opts
    );

    // Decoder
    AVBufferRef *decoder = tx_create_decoder(
        ctx,
        "vp9", // dec_name
        NULL // init_opts
    );

    err = tx_link(ctx, demuxer, decoder, 0);
    assert(err == 0);

    // Encoder
    AVDictionary *encoder_options = NULL;
    err = av_dict_set(&encoder_options, "b", "10M", 0);

    AVBufferRef *encoder = tx_create_encoder(
        ctx,
        "libx264",
        NULL, // name
        &encoder_options,
        NULL // init_opts
    );

    err = tx_link(ctx, decoder, encoder, 0);
    assert(err == 0);

    // Muxer
    AVBufferRef *muxer = tx_create_muxer(
        ctx,
        "test-transcoded.mkv",
        NULL // init_opts
    );

    err = tx_link(ctx, encoder, muxer, 0);
    assert(err == 0);

    // Setup End of stream handler
    AVBufferRef *muxer_eof_event = NULL;
    muxer_eof_event = sp_event_create(muxer_eos_cb,
                                      NULL,
                                      0,
                                      NULL,
                                      SP_EVENT_ON_EOS,
                                      NULL,
                                      NULL);

    err = tx_register_event(ctx, muxer, muxer_eof_event);
    assert(err == 0);

    // Commit
    err = tx_commit(ctx);
    assert(err == 0);

    while (1)
        av_usleep(UINT_MAX);

    av_buffer_unref(&muxer_eof_event);
    tx_free(ctx);

    return 0;
}
