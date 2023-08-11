#include <stdio.h>
#include <assert.h>
#include <limits.h>
#include <pthread.h>

#include <libavutil/buffer.h>
#include <libavutil/time.h>

#include <libtxproto/events.h>
#include <libtxproto/txproto.h>

// gcc -Wall -g broadcast_video_dual_rtp.c -o broadcast_video_dual_rtp `pkg-config --cflags --libs txproto libavutil`

static int muxer_eos_cb(AVBufferRef *event_ref, void *callback_ctx, void *ctx,
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
        "test.mkv", // in_url
        NULL, // in_format
        NULL, // start_options
        NULL // init_opts
    );

    // Decoder
    AVBufferRef *decoder = tx_create_decoder(
        ctx,
        "h264_cuvid",
        NULL // init_opts
    );

    err = tx_link(ctx, demuxer, decoder, 0);
    assert(err == 0);

    // Encoder
    AVDictionary *encoder_options = NULL;
    err = av_dict_set(&encoder_options, "b", "20M", 0);
    assert(err == 0);

    err = av_dict_set(&encoder_options, "bf", "0", 0);
    assert(err == 0);

    AVBufferRef *encoder = tx_create_encoder(
        ctx,
        "h264_nvenc",
        NULL, // name
        &encoder_options,
        NULL // init_opts
    );

    err = tx_link(ctx, decoder, encoder, 0);
    assert(err == 0);

    // Muxer
    AVDictionary *muxer_options = NULL;
    err = av_dict_set_int(&muxer_options, "low_latency", 1, 0);
    assert(err == 0);

    err = av_dict_set(&muxer_options, "sdp_file", "video.sdp", 0);
    assert(err == 0);

    AVBufferRef *muxer = tx_create_muxer(
        ctx,
        "rtp://127.0.0.1:30000",
        "rtp",
        muxer_options
    );

    err = tx_link(ctx, encoder, muxer, 0);
    assert(err == 0);

    // Commit
    err = tx_commit(ctx);
    assert(err == 0);

    // Wait to send stream the video to another
    while (1) {
        av_usleep(5000000); // 5 seconds
        printf("Start another mux\n");

        // Filter
        AVDictionary *filter_options = NULL;
        err = av_dict_set_int(&filter_options, "dump_graph", 1, 0);
        assert(err == 0);

        AVBufferRef *filter = tx_create_filtergraph(
            ctx,
            "negate",
            AV_HWDEVICE_TYPE_CUDA,
            filter_options);

        err = tx_link(ctx, decoder, filter, 0);
        assert(err == 0);

        // Encoder
        AVDictionary *encoder2_options = NULL;
        err = av_dict_set(&encoder2_options, "b", "5M", 0);
        assert(err == 0);

        err = av_dict_set(&encoder2_options, "bf", "0", 0);
        assert(err == 0);

        AVBufferRef *encoder2 = tx_create_encoder(
            ctx,
            "h264_nvenc",
            NULL, // name
            &encoder2_options,
            NULL // init_opts
        );

        err = tx_link(ctx, filter, encoder2, 0);
        assert(err == 0);

        AVDictionary *muxer2_options = NULL;
        err = av_dict_set_int(&muxer2_options, "low_latency", 1, 0);
        assert(err == 0);

        err = av_dict_set(&muxer2_options, "sdp_file", "video2.sdp", 0);
        assert(err == 0);

        AVBufferRef *muxer2 = tx_create_muxer(
            ctx,
            "rtp://127.0.0.1:30010",
            "rtp",
            muxer2_options
        );

        err = tx_link(ctx, encoder2, muxer2, 0);
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

        err = tx_register_event(ctx, muxer2, muxer_eof_event);
        assert(err == 0);

        err = tx_commit(ctx);
        assert(err == 0);

        // Stop second mux
        av_usleep(20000000); // 20 seconds
        printf("Stop second stream\n");

        err = tx_destroy(ctx, &filter);
        assert(err == 0);

        av_usleep(3000000); // 3 seconds
        printf("Stop second stream: Done\n");

        err = tx_destroy(ctx, &encoder2);
        assert(err == 0);

        err = tx_destroy(ctx, &muxer2);
        assert(err == 0);

        av_buffer_unref(&muxer_eof_event);
    }

    tx_free(ctx);

    return 0;
}
