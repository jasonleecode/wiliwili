/* Reproduce mpv retaining a displayed hardware frame across decoder flush. */
#include <stdio.h>
#include <stdlib.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavutil/time.h>

static int next_frame(AVFormatContext *format, AVCodecContext *codec, int stream,
                      AVPacket *packet, AVFrame *frame)
{
    for (;;) {
        int ret = avcodec_receive_frame(codec, frame);
        if (ret != AVERROR(EAGAIN)) return ret;
        if (!packet->size) {
            do {
                av_packet_unref(packet);
                ret = av_read_frame(format, packet);
                if (ret < 0) return ret;
            } while (packet->stream_index != stream);
        }
        ret = avcodec_send_packet(codec, packet);
        if (ret == 0) av_packet_unref(packet);
        else if (ret != AVERROR(EAGAIN)) return ret;
    }
}

static long cma_free(void)
{
    FILE *f = fopen("/proc/meminfo", "r");
    char line[256]; long value = -1;
    if (!f) return value;
    while (fgets(line, sizeof(line), f))
        if (sscanf(line, "CmaFree: %ld kB", &value) == 1) break;
    fclose(f);
    return value;
}

int main(int argc, char **argv)
{
    if (argc != 4) return 2;
    av_log_set_level(AV_LOG_ERROR);
    AVFormatContext *format = NULL;
    AVCodecContext *codec = NULL;
    AVFrame *frame = av_frame_alloc(), *held = NULL;
    AVPacket *packet = av_packet_alloc();
    int ret = avformat_open_input(&format, argv[1], NULL, NULL);
    if (ret < 0) goto done;
    ret = avformat_find_stream_info(format, NULL);
    if (ret < 0) goto done;
    int stream = av_find_best_stream(format, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (stream < 0) { ret = stream; goto done; }
    const AVCodec *decoder = avcodec_find_decoder_by_name("h264_v4l2m2m");
    codec = avcodec_alloc_context3(decoder);
    avcodec_parameters_to_context(codec, format->streams[stream]->codecpar);
    av_opt_set_int(codec->priv_data, "num_capture_buffers", atoi(argv[2]), 0);
    ret = av_opt_set_int(codec->priv_data, "n1_copy_frames", atoi(argv[3]), 0);
    if (ret < 0) goto done;
    ret = avcodec_open2(codec, decoder, NULL);
    if (ret < 0) goto done;
    ret = next_frame(format, codec, stream, packet, frame);
    if (ret < 0) goto done;
    held = av_frame_clone(frame);
    av_frame_unref(frame);
    printf("Initial frame, CmaFree=%ld KiB\n", cma_free()); fflush(stdout);
    const int targets[] = {12, 3, 24, 5, 18, 1};
    for (unsigned i = 0; i < sizeof(targets)/sizeof(targets[0]); i++) {
        int64_t target = av_rescale_q(targets[i] * (int64_t)AV_TIME_BASE,
                                      AV_TIME_BASE_Q, format->streams[stream]->time_base);
        ret = av_seek_frame(format, stream, target, AVSEEK_FLAG_BACKWARD);
        if (ret < 0) goto done;
        av_packet_unref(packet);
        avcodec_flush_buffers(codec);
        int64_t start = av_gettime_relative();
        ret = next_frame(format, codec, stream, packet, frame);
        if (ret < 0) goto done;
        printf("Seek %d: frame %dx%d, held old frame, %.3fs, CmaFree=%ld KiB\n",
               targets[i], frame->width, frame->height,
               (av_gettime_relative() - start) / 1e6, cma_free()); fflush(stdout);
        av_frame_free(&held);
        held = av_frame_clone(frame);
        av_frame_unref(frame);
    }
done:
    av_frame_free(&held); av_frame_free(&frame); av_packet_free(&packet);
    avcodec_free_context(&codec); avformat_close_input(&format);
    if (ret < 0) {
        char error[AV_ERROR_MAX_STRING_SIZE]; av_strerror(ret, error, sizeof(error));
        fprintf(stderr, "FAILED: %s\n", error);
        return 1;
    }
    puts("All held-frame seek tests passed");
    return 0;
}
