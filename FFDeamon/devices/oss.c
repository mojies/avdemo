#include <libavformat/avformat.h>
#include "audio_device.h"

typedef struct{
    e_au_dev    a_dev;
    int         a_rate;
    int         a_channel;
}mOSSData;

static int audio_write_header(AVFormatContext *s1)
{
    mOSSData       *tp_oss = s1->priv_data;
    m_au_conf       tv_conf;

    tp_oss->a_dev = gf_audev_get_devid_from_name( s1->filename );
    tp_oss->a_rate = s1->streams[0]->codecpar->sample_rate;
    tp_oss->a_channel = s1->streams[0]->codecpar->channels;

    tv_conf.a_status = 0;
    tv_conf.a_status |= DC_AUDEV_SET_VOL;
    tv_conf.a_status |= DC_AUDEV_SET_CHANNEL;
    tv_conf.a_status |= DC_AUDEV_SET_FMT;
    tv_conf.a_status |= DC_AUDEV_SET_RATE;
    tv_conf.a_status |= DC_AUDEV_SET_FRAGMENT;
    tv_conf.a_status |= DC_AUDEV_SET_SPEEK;
    tv_conf.a_status |= DC_AUDEV_SET_FORCE;
    tv_conf.a_num       = tp_oss->a_dev;
    tv_conf.a_channel   = tp_oss->a_channel;
    tv_conf.a_rate      = tp_oss->a_rate;
    tv_conf.a_pvol      = 100;
    // cent 2 16
    // size 2^4 - 2^13 ( 16 8k )
    tv_conf.a_fragment = 12|(8<<16); // 2^12 + 8
    // tv_conf.a_fragment = 13|(8<<16); // 2^10 + 8
    if( gf_audev_config( &tv_conf ) )
        return AVERROR(EIO);
    else
        return 0;
}

static int audio_write_packet(AVFormatContext *s1, AVPacket *pkt)
{
    mOSSData       *tp_oss = s1->priv_data;
    int             size = pkt->size;
    uint8_t        *buf = pkt->data;

    gf_audev_block_play( tp_oss->a_dev, buf, size );

    return 0;
}

static int audio_write_trailer(AVFormatContext *s1)
{
    mOSSData       *tp_oss = s1->priv_data;
    gf_audev_release_devices( tp_oss->a_dev );
    return 0;
}

AVOutputFormat ff_oss_muxer = {
    .name           = "oss",
    .long_name      = NULL,
    .priv_data_size = sizeof(mOSSData),
    /* XXX: we make the assumption that the soundcard accepts this format */
    /* XXX: find better solution with "preinit" method, needed also in
     *        other formats */
    .audio_codec    = AV_NE(AV_CODEC_ID_PCM_S16BE, AV_CODEC_ID_PCM_S16LE),
    .video_codec    = AV_CODEC_ID_NONE,
    .write_header   = audio_write_header,
    .write_packet   = audio_write_packet,
    .write_trailer  = audio_write_trailer,
    .flags          = AVFMT_NOFILE,
};

