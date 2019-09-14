#include <pthread.h>
#include "player.h"
#include "../tools/Debug.h"
#include "devices/audio_device.h"

// -----------------------------------------------------------------------------
typedef struct mAudioFormat{
    uint64_t                a_clayout;
    int                     a_fmt;
    int                     a_rate;
}mAudioFormat;

typedef struct mAudioOutputOSS{
    AVFormatContext    *a_ctx;
}mAudioOutputOSS;

typedef struct mAudioStream{
    int                 a_index;
    uint32_t            a_status;
#define L_STREAM_STA_ACTIVE     0x01
#define L_STREAM_STA_STOP       0x02
#define L_STREAM_STA_PAUSE      0x04

    AVFormatContext    *a_ctx;
    AVCodecContext     *a_codec_ctx;
    AVFilterGraph      *a_filtergraph;
    AVFilterContext    *a_filtsrc;
    AVFilterContext    *a_filtdst;
    AVPacket           *a_pkg;
    AVFrame            *a_frame;
    int                 a_austream_id; // index in stream

    int16_t            *a_data;
    int16_t             a_len;
    int16_t             a_vol;
    int16_t             a_scale;
    int (*af_recycle)( struct mAudioStream * );
    int (*af_decode)( struct mAudioStream * );
}mAudioStream;

// -----------------------------------------------------------------------------
static int sf_open_input_file( const char *ifile_name, mAudioStream *istream );
static int sf_stream_recycle( struct mAudioStream *istream );
static int sf_stream_decode( struct mAudioStream *istream );
static void *sf_player_processor( void *pdata );
static int sf_configure_filtergraph( mAudioStream *istream, AVCodecParameters *icodecpar );

// -----------------------------------------------------------------------------
static unsigned             sv_status = 0;
static mAudioStream        *sp_stream = NULL;
static int                  sv_stream_nbs = 0;
static pthread_mutex_t      sv_mutex;
static pthread_cond_t       sv_cond;
static pthread_t            sv_play_thrdid; // player thrd id
static mAudioFormat         sv_output_format;

// -----------------------------------------------------------------------------
int player_init( int inbs_stream, uint64_t iochannel, int iofmt, int iorate ){
    int i;

    if( inbs_stream <= 0  ) return -1;
    if( ( sp_stream = malloc( sizeof( mAudioStream )*inbs_stream ) ) == NULL ){
        DLLOGE( "play stream equest memory failed!" );
        return -1;
    }
    memset( sp_stream, 0x00, sizeof( mAudioStream ) );
    sv_stream_nbs = inbs_stream;
    for( i = 0; i < sv_stream_nbs; i++ ){
        sp_stream[ i ].a_index = i;
        sp_stream[ i ].af_recycle = sf_stream_recycle;
        sp_stream[ i ].af_decode = sf_stream_decode;
        sp_stream[ i ].a_pkg = (AVPacket*)av_malloc( sizeof( AVPacket ) );
        sp_stream[ i ].a_frame = av_frame_alloc();
        // DLLOGE( "initial stream %d %p", i, sp_stream[ i ].a_pkg );
    }

    pthread_mutex_init( &sv_mutex, NULL );
    pthread_cond_init( &sv_cond, NULL );

    sv_output_format.a_clayout = iochannel;
    sv_output_format.a_fmt = iofmt;
    sv_output_format.a_rate = iorate;
    pthread_create( &sv_play_thrdid, NULL, sf_player_processor, NULL );
    return 0;
err:
    if( sp_stream )
        free( sp_stream );
    return -1;
}

#if 0
static volatile int received_nb_signals = 0;
static int decode_interrupt_cb(void *ctx)
{
    return received_nb_signals > 1;
}
static AVFormatContext     *sp_alsa_ctx = NULL;
static AVOutputFormat      *sp_alsa_format = NULL;
const AVIOInterruptCB int_cb = { decode_interrupt_cb, NULL };
#endif

#if 1
int player_open_output_device( int ien_oss, int ien_alsa, int ien_uds ){
    if( ien_oss ){
        int         tv_to_count = 0;

        DLLOGE( "initial output oss" );
        m_au_conf       tv_conf;

        tv_conf.a_status = 0;
        tv_conf.a_status |= DC_AUDEV_SET_VOL;
        tv_conf.a_status |= DC_AUDEV_SET_CHANNEL;
        tv_conf.a_status |= DC_AUDEV_SET_FMT;
        tv_conf.a_status |= DC_AUDEV_SET_RATE;
        tv_conf.a_status |= DC_AUDEV_SET_FRAGMENT;
        tv_conf.a_status |= DC_AUDEV_SET_SPEEK;
        tv_conf.a_status |= DC_AUDEV_SET_FORCE;
        // tv_conf.a_num       = gf_audev_get_devid_from_name();
        tv_conf.a_fmt       = AUDIO_FMT_S16_LE;
        tv_conf.a_num       = AUDIO_DEVICE_0;
        tv_conf.a_channel   = av_get_channel_layout_nb_channels(\
                sv_output_format.a_clayout );
        tv_conf.a_rate      = sv_output_format.a_rate;
        tv_conf.a_pvol      = 100;
        // cent 2 16
        // size 2^4 - 2^13 ( 16 8k )
        tv_conf.a_fragment = 12|(8<<16); // 2^12 + 8
        // tv_conf.a_fragment = 13|(8<<16); // 2^10 + 8
        while( gf_audev_config( &tv_conf ) && tv_to_count < 10 ){
            DLLOGE( "initial oss failed!" );
            usleep( 50000 );
            tv_to_count++;
        }
        if( tv_to_count >= 10 )
            return -1;
    }
    if( ien_alsa ){
        DLLOGE( "initial output alsa" );
    }
    if( ien_uds ){
        DLLOGE( "initial output uds" );
    }
}
#else
int player_open_output_device( int ien_oss, int ien_alsa, int ien_uds ){

    if( ien_oss ){
        AVFormatContext     *tp_format_context;
        AVOutputFormat      *tp_oformat;
        AVStream            *tp_st;
        AVDictionary        *tp_dictionary;
        if( ( tp_format_context = avformat_alloc_context() ) == NULL ){
            DLLOGE( "%d", __LINE__ );
            return -1;
        }

        tp_oformat = av_guess_format( "oss", NULL, NULL);
        oc->oformat = tp_oformat;
        oc->interrupt_callback = int_cb;

        av_strlcpy( oc->filename, "/dev/dsp", sizeof("/dev/dsp") );;

        if( ( tp_st = avformat_new_stream( tp_format_context, NULL ) ) == NULL ){
            DLLOGE( "%d", __LINE__ );
            return -1;
        }

        tp_st->codecpar->codec_type = AVMEDIA_TYPE_AUDIO;
        tp_st->codecpar->codec_id = av_guess_codec( tp_format_context->oformat, \
                NULL, tp_format_context->filename,\
                NULL, tp_st->codecpar->codec_type);
        tp_st->codecpar->sample_rate = 48000;
        tp_st->codecpar->channels = 1;
        avformat_write_header( tp_format_context, &tp_dictionary );
    }
    if( ien_alsa ){
        if( ( sp_alsa_ctx = avformat_alloc_context() ) == NULL ){
            DLLOGE( "%d", __LINE__ );
            return -1;
        }

        sp_alsa_format = av_guess_format( "oss", NULL, NULL);
        sp_alsa_ctx->oformat = tp_oformat;
        sp_alsa_ctx->interrupt_callback = int_cb;

        av_strlcpy( sp_alsa_ctx->filename, "/dev/dsp", sizeof("/dev/dsp") );

        if( ( tp_st = avformat_new_stream( tp_format_context, NULL ) ) == NULL ){
            DLLOGE( "%d", __LINE__ );
            return -1;
        }

        tp_st->codecpar->codec_type = AVMEDIA_TYPE_AUDIO;
        tp_st->codecpar->codec_id = av_guess_codec( tp_format_context->oformat, \
                NULL, tp_format_context->filename,\
                NULL, tp_st->codecpar->codec_type);
        tp_st->codecpar->sample_rate = 48000;
        tp_st->codecpar->channels = 1;
        avformat_write_header( tp_format_context, &tp_dictionary );

    }
    if( ien_uds ){
    
    }
}
#endif

int player_play( int istream, char const *iname ){
    AVCodecParameters      *tp_codecpar = NULL;
    AVCodec                *tp_codec;
    mAudioStream           *tp_stream;
    int                     tv_ret;
    unsigned                i;

    if( sp_stream == NULL ) return -1;
    tp_stream = &sp_stream[ istream ];
    // DLLOGI( "-------------> 1 %p %d %p", tp_stream, istream, tp_stream->a_pkg );

    if( tp_stream->a_status & L_STREAM_STA_ACTIVE ){
        // stop play
        return -1;
    }

#if 0
    if( ( tp_stream->a_ctx = avformat_alloc_context() ) == NULL ){
        DLLOGE( "%s %s %d", __FILE__, __func__, __LINE__ );
        return -1;
    }

    if( avformat_open_input( &(tp_stream->a_ctx), iname, NULL, NULL ) != 0 ){
        DLLOGE( "Couldn't open input stream." );
        return -1;
    }

    if( avformat_find_stream_info( tp_stream->a_ctx, NULL ) < 0 ){
        DLLOGE( "Couldn't find stream information." );
        return -1;
    }
    for( i = 0; i < tp_stream->a_ctx->nb_streams; i++ )
        if( tp_stream->a_ctx->streams[ i ]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO ){
            tp_stream->a_austream_id = i;
            break;
        }
    if( i == tp_stream->a_ctx->nb_streams ){
        DLLOGE( "didnt fund audio stream!" );
        return -1;
    }

    tp_codecpar = tp_stream->a_ctx->streams[ tp_stream->a_austream_id ]->codecpar;
    if( ( tp_codec = avcodec_find_decoder( tp_codecpar->codec_id ) ) == NULL ){
        return -1;
    }
    if( ( tp_stream->a_codec_ctx = avcodec_alloc_context3( tp_codec ) ) == NULL ){
        return -1;
    }
    if( avcodec_parameters_to_context( tp_stream->a_codec_ctx, tp_codecpar ) < 0 ){
        return -1;
    }

    if( avcodec_open2( tp_stream->a_codec_ctx, tp_codec, NULL ) < 0 ){
        return -1;
    }
#endif
    if( ( tv_ret = sf_open_input_file( iname, tp_stream ) ) != 0 ){
        DLLOGE( "Open input failed : %d", tv_ret );
        return -1;
    }

    if( ( tv_ret = sf_configure_filtergraph( tp_stream, \
            tp_stream->a_ctx->streams[ tp_stream->a_austream_id ]->codecpar )) < 0 ){
        DLLOGE( "Config input filter graph faile : %d", tv_ret );
        return -1;
    }
    // tp_codecpar = tp_stream->a_ctx->streams[ tp_stream->a_austream_id ]->codecpar;
    // DLLOGD( "-----> %d - %llu", __LINE__, tp_codecpar->channel_layout );
    // DLLOGD( "-----> %d %p", __LINE__, tp_stream->a_pkg );
    av_init_packet( tp_stream->a_pkg );
    av_frame_unref( tp_stream->a_frame );
    // DLLOGD( "-----> %d", __LINE__ );
    tp_stream->a_status = L_STREAM_STA_ACTIVE;
    pthread_cond_broadcast( &sv_cond );
}


#if 0
int player_config( int istream, unsigned icmd,  ){

    switch( icmd ){
    case G_PLAYER_CMD_PLAY:
        // create stream
        pthread_cond_broadcast( &sv_cond );
    break;
    case G_PLAYER_CMD_STOP:
        // delete stream
    break;
    case G_PLAYER_CMD_PAUSE:
        tp_stream->status |= L_PLAYER_STREAM_STA_PAUSE;
    break;
    case G_PLAYER_CMD_RESUME:
        tp_stream->status &= ~L_PLAYER_STREAM_STA_PAUSE;
        pthread_cond_broadcast( &sv_cond );
    break;
    default: return -1; break;
    }
    return 0;
}
#endif

static void *sf_player_processor( void *pdata ){
    mAudioStream          **tp_stream;
    int                     tv_need_play;
    int16_t                 tv_aubuf[ 4096 ]; // audio buffer
    int                     tv_plen; // play samples
    int                     tv_stream_nb;
    int                     i, j;

    pthread_detach( pthread_self() );
    if( ( tp_stream = malloc( sizeof( mAudioStream* )*sv_stream_nbs ) ) == NULL ){
        DLLOGE( "%s %s %d request memory failed", __FILE__, __func__, __LINE__  );
        goto thrd_end;
    }
    tv_stream_nb = 0;

    while(1){
        tv_need_play = 0;
        // clean useless stream
        for( i = 0, j = 0; i < tv_stream_nb; i++  ){
            if( tp_stream[ i ]->a_status & L_STREAM_STA_STOP ){
                // recycling resources
                tp_stream[ i ]->af_recycle( tp_stream[ i ] );
                // kick out tv_stream
                tp_stream[ i ] = NULL;
                continue;
            }
            if( tp_stream[ i ]->a_status & L_STREAM_STA_PAUSE ){
                tp_stream[ i ] = NULL;
                continue;
            }
            if( tp_stream[ i ] ){
                tp_stream[ j ] = tp_stream[ i ];
                j++;
            }
        }
        tv_stream_nb = j;

        // add alernate stream
        for( i = 0; i < sv_stream_nbs; i++  ){
            if( sp_stream[ i ].a_status & L_STREAM_STA_ACTIVE ){
                for( j = 0; j < tv_stream_nb; j++ )
                    if( tp_stream[ j ] == &sp_stream[i] )
                        continue;
                tp_stream[ tv_stream_nb ] = &sp_stream[i];
                tv_stream_nb++;
            }
        }

        if( tv_stream_nb == 0 ) goto thrd_play_or_sleep;
        else tv_need_play = 1;

        DLLOGD( "--->4" );
        // streeam decode
        for( i = 0; i < tv_stream_nb; i++ ){
            tp_stream[ i ]->af_decode( tp_stream[ i ] );
        }
        DLLOGD( "--->5" );
#if 0
        // mix len, mix scale
        {
            int                 tv_mixlen;
            int                 tv_maxvol;
            int                 tv_sumvol;

            DLLOGI( "INT MAX is %d", INT_MAX );
            tv_mixlen = INT_MAX;
            tv_maxvol = 0;
            tv_sumvol = 0;
            for( i = 0; i < tv_stream_nb; i++ ){
                if( tp_stream[ i ]->a_len < tv_mixlen )
                    tv_mixlen = tp_stream[ i ]->a_len;
                if( tp_stream[ i ]->a_vol > tv_maxvol )
                    tv_maxvol = tp_stream[ i ]->a_vol;
                tv_sumvol += tp_stream[ i ]->a_vol;
            }
            // tv_maxvol += tv_stream_nb;
            for( i = 0; i < tv_stream_nb; i++ )
                tp_stream[ i ]->a_scale = tv_maxvol*tp_stream[ i ]->a_vol/tv_sumvol;

            // mix
            for( tv_plen = 0; tv_plen < tv_mixlen; tv_plen++  ){
                tv_aubuf[ tv_plen ] = 0;
                for( i = 0; i < tv_stream_nb; i++ )
                    tv_aubuf[ tv_plen ] += ((int32_t)tp_stream[ i ]->a_data[ tv_plen ])*tp_stream[ i ]->a_scale/100;
            }
        }
#endif

thrd_play_or_sleep:
        // output
        if( tv_need_play ){
            DLLOGI( "play something" );
            // write data to output
            // DLLOGI( "write audio data size : %d", tv_plen );
            // player_write( tv_aubuf, tv_plen );
        }else{
            DLLOGI( "sleeping" );
            pthread_cond_wait( &sv_cond, &sv_mutex );
        }
    }
thrd_end:
    sv_play_thrdid = 0;
    pthread_exit( NULL );
}

// -----------------------------------------------------------------------------
static int sf_stream_recycle( struct mAudioStream *istream ){
    mAudioStream                *tp_stream;
    if( istream == NULL ) return -1;
    tp_stream = istream;

    DLLOGI( "sf_stream_recycle" );
    tp_stream->a_status = 0;
    // free( tp_stream->a_data );
    tp_stream->a_data = NULL;
    tp_stream->a_len = 0;
    tp_stream->a_vol = 0;
    tp_stream->a_scale = 0;

    // ----------------------> free filter graph
    avfilter_graph_free( &( tp_stream->a_filtergraph ) );

    // ----------------------> free codec
    avcodec_free_context( &tp_stream->a_codec_ctx );
    tp_stream->a_codec_ctx = NULL;
    // av_frame_free( &tp_stream->a_frame );
    // tp_stream->a_frame = NULL;
    // av_packet_free( &tp_stream->a_pkg );
    // tp_stream->a_pkg = NULL;

    // ----------------------> free context
    avformat_close_input( &tp_stream->a_ctx );
    // tp_stream->a_ctx = NULL;
    return 0;
}

static int sf_stream_decode( struct mAudioStream *istream ){
    mAudioStream                *tp_stream;
    AVPacket                     tv_pkg;
    int                          tv_ret;
    static int                   tv_size = 0;

    if( istream == NULL ) return -1;
    tp_stream = istream;

get_more_frame:
    av_init_packet( tp_stream->a_pkg );
    tp_stream->a_pkg->data = NULL;
    tp_stream->a_pkg->size = 0;

    // DLLOGD( "-->1 %p %p ", tp_stream->a_ctx, tp_stream->a_pkg );
    tv_ret = av_read_frame( tp_stream->a_ctx, tp_stream->a_pkg );
    // tv_ret = av_read_frame( tp_stream->a_ctx, &tv_pkg );
    // DLLOGD( "-->2" );
    if( tv_ret == AVERROR(EAGAIN) ){
        DLLOGE( "%s %s %d", __FILE__, __func__, __LINE__ );
        goto get_more_frame;
    }else if( tv_ret < 0 ){
        DLLOGE( "%s %s %d", __FILE__, __func__, __LINE__ );
        tp_stream->a_status |= L_STREAM_STA_STOP;
        return tv_ret;
    }
    if( tp_stream->a_pkg->stream_index == tp_stream->a_austream_id ){
        tv_size += tp_stream->a_pkg->size;
        DLLOGI( "- pkg -> size : %d - %d", tp_stream->a_pkg->size, tv_size );
        DLLOGD( "----> 1" );
        tv_ret = avcodec_send_packet( tp_stream->a_codec_ctx, tp_stream->a_pkg );
        DLLOGD( "----> 2" );
        // av_packet_unref( tp_stream->a_pkg );
        if( tv_ret < 0 ){
            DLLOGE( "%s %s %d", __FILE__, __func__, __LINE__ );
            return tv_ret == AVERROR_EOF ? 0 : tv_ret;
        }
        while(1){
            tv_ret = avcodec_receive_frame( tp_stream->a_codec_ctx,\
                    tp_stream->a_frame );
            if( tv_ret == AVERROR(EAGAIN) ){
                DLLOGE( "Need more data" );
                return 0;
            }else if( tv_ret < 0 ){
                DLLOGE( "Serious error" );
                return tv_ret;
            }else if( tv_ret >= 0 ){
                // static int tv_fd = -1;
                // DLLOGE( "got a frame data" );
                // resample
                if( ( tv_ret = av_buffersrc_add_frame( tp_stream->a_filtsrc,\
                                tp_stream->a_frame ) ) < 0 ){

                    DLLOGE( "add frame to graph failed : %d", tv_ret );
                    av_frame_unref( tp_stream->a_frame );
                    return tv_ret;
                }
                av_frame_unref( tp_stream->a_frame );
                while( 1 ){
                    AVFrame    *tp_frame;

                    tp_frame = av_frame_alloc();
                    if( ( tv_ret = av_buffersink_get_frame( tp_stream->a_filtdst,\
                                    tp_frame ) ) >= 0 ) {

                        gf_audev_block_play( AUDIO_DEVICE_0, tp_frame->data[0],\
                                tp_stream->a_frame->nb_samples*2 );

                        av_frame_free( &tp_frame );
                    }else{
                        if( tv_ret == AVERROR(EAGAIN) ){
                            tv_ret = 0;
                            goto GT_decode_end;
                        }
                        DLLOGE( "get frame to graph failed : %d", tv_ret );
                        return tv_ret;
                    }
                }
            }
        }
    }else{
        av_packet_unref( tp_stream->a_pkg );
    }
GT_decode_end:
    av_frame_unref( tp_stream->a_frame );
    return 0;
}

static int sf_player_out_write( uint8_t *iaudata, uint32_t ilen  ){
    
}

static int output_write( void *iplay, uint8_t ibuf, int ilen ){

    /*
    if( oss )
        gf_audev_block_play( tp_output->fd, ibuf, ilen );
    else if( uds )
        write( tp_output->fd, ibuf, ilen );
        */
    // else if( alsa )
    
}

static int sf_open_input_file( const char *ifile_name, mAudioStream *istream ){
    AVFormatContext    *tp_ctx = NULL;
    AVCodecContext     *tp_codec_ctx = NULL;
    AVCodec            *tp_codec = NULL;
    int                 tv_streamid = 0;
    int                 tv_ret = 0;
    unsigned            i;

    /** Open the input file to read from it. */
    if( ( tv_ret = avformat_open_input( &tp_ctx, ifile_name, NULL,
                                     NULL ) ) < 0 ){
        /* fprintf(stderr, "Could not open input file '%s' (tv_ret '%s')\n",
                ifile_name, av_err2str(tv_ret)); */
        tp_ctx = NULL;
        return tv_ret;
    }

    /** Get information on the input file (number of streams etc.). */
    if( ( tv_ret = avformat_find_stream_info( tp_ctx, NULL ) ) < 0 ){
        /* fprintf(stderr, "Could not open find stream info (tv_ret '%s')\n",
                av_err2str(tv_ret)); */
        goto GT_open_input_file_faile;
    }

    /** Make sure that there is only one stream in the input file. */
    for( i = 0; i < tp_ctx->nb_streams; i++ ){
        if( tp_ctx->streams[ i ]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO ){
            tv_streamid = i;
            break;
        }
    }
    if( i == tp_ctx->nb_streams ){
        fprintf(stderr, "Expected one audio input stream, but found %d\n",
                (tp_ctx)->nb_streams);
        tv_ret = AVERROR_EXIT;
        goto GT_open_input_file_faile;
    }

    /** Find a decoder for the audio stream. */
    if( !( tp_codec = avcodec_find_decoder(\
                    tp_ctx->streams[ tv_streamid ]->codecpar->codec_id ) ) ){
        fprintf(stderr, "Could not find input codec\n");
        tv_ret = AVERROR_EXIT;
        goto GT_open_input_file_faile;
    }

    /** allocate a new decoding context */
    tp_codec_ctx = avcodec_alloc_context3( tp_codec );
    if( !tp_codec_ctx ){
        fprintf(stderr, "Could not allocate a decoding context\n");
        tv_ret = AVERROR_EXIT;
        goto GT_open_input_file_faile;
    }

    /** initialize the stream parameters with demuxer information */
    tv_ret = avcodec_parameters_to_context( tp_codec_ctx,\
            tp_ctx->streams[ tv_streamid ]->codecpar );
    if( tv_ret < 0 ){
        goto GT_open_input_file_faile;
    }

    /** Open the decoder for the audio stream to use it later. */
    if ((tv_ret = avcodec_open2(tp_codec_ctx, tp_codec, NULL)) < 0) {
        /* fprintf(stderr, "Could not open input codec (tv_ret '%s')\n",
                av_err2str(tv_ret)); */
        goto GT_open_input_file_faile;
    }

    if( !tp_codec_ctx->channel_layout )
        tp_codec_ctx->channel_layout = av_get_default_channel_layout(\
                tp_codec_ctx->channels);

    /** Save the decoder context for easier access later. */
    istream->a_ctx = tp_ctx;
    istream->a_codec_ctx = tp_codec_ctx;
    istream->a_austream_id = tv_streamid;

    return 0;
GT_open_input_file_faile:
    if( tp_codec_ctx )
        avcodec_free_context( &tp_codec_ctx );
    if( tp_ctx )
        avformat_close_input( &tp_ctx );
    return tv_ret;
}

static int sf_configure_filtergraph( mAudioStream *istream, AVCodecParameters *icodecpar ){
    AVFilterGraph          *tp_graph = NULL;
    AVFilter               *tp_filt_src;
    AVFilter               *tp_filt_volume;
    AVFilter               *tp_filt_format;
    AVFilter               *tp_filt_dst;
    AVFilterContext        *tp_ctx_last;
    AVFilterContext        *tp_ctx_src;
    AVFilterContext        *tp_ctx_format;
    AVFilterContext        *tp_ctx_dst;
    AVFilterInOut          *tp_input = NULL;
    AVFilterInOut          *tp_output = NULL;
    int                     tv_ret;

    if( istream == NULL ) return -1;
    if( ( tp_graph = avfilter_graph_alloc() ) == NULL ){
        tv_ret = AVERROR(ENOMEM);
        goto GT_configure_filtegraph_fail;
    }

    if( ( tv_ret = avfilter_graph_parse2( tp_graph, "anull",\
                    &tp_input, &tp_output) ) < 0 ){
        goto GT_configure_filtegraph_fail;
    }

    // --------------------> source
    if( !(tp_filt_src = avfilter_get_by_name("abuffer")) ){
        DLLOGE( "failed get resample!" );
        tv_ret = AVERROR(EINVAL);
        goto GT_configure_filtegraph_fail;
    }
    if( ( tp_ctx_src = avfilter_graph_alloc_filter( \
                    tp_graph, tp_filt_src, "src") ) == NULL ){
        tv_ret = AVERROR(ENOMEM);
        goto GT_configure_filtegraph_fail;
    }
    {
        AVBufferSrcParameters   *tp_param;
        if( ( tp_param = av_buffersrc_parameters_alloc() ) == NULL ){
            tv_ret = AVERROR(ENOMEM);
            goto GT_configure_filtegraph_fail;
        }

        tp_param->time_base      = (AVRational){ 1, icodecpar->sample_rate };
        tp_param->sample_rate    = icodecpar->sample_rate;
        tp_param->format         = icodecpar->format;
        tp_param->channel_layout = av_get_default_channel_layout(\
                icodecpar->channels );
        // tp_param->channel_layout = icodecpar->channel_layout;
        tv_ret = av_buffersrc_parameters_set( tp_ctx_src, tp_param);
        av_freep(&tp_param);
    }
    if( ( tv_ret = avfilter_init_str( tp_ctx_src, NULL ) ) < 0 ){
        goto GT_configure_filtegraph_fail;
    }
    tp_ctx_last = tp_ctx_src;
    // --------------------> link filt input
    if( ( tv_ret = avfilter_link( tp_ctx_last, 0,\
                    tp_input->filter_ctx, tp_input->pad_idx  ) ) < 0 ){
        goto GT_configure_filtegraph_fail;
    }

    // --------------------> format
    if( !(tp_filt_format = avfilter_get_by_name("aformat")) ){
        DLLOGE( "failed get resample!" );
        tv_ret = AVERROR(EINVAL);
        goto GT_configure_filtegraph_fail;
    }
    if( ( tp_ctx_format = avfilter_graph_alloc_filter(\
                    tp_graph, tp_filt_format, "aformat") ) == NULL ){
        tv_ret = AVERROR(ENOMEM);
        goto GT_configure_filtegraph_fail;
    }
    {
        uint8_t options_str[1024];
        snprintf( options_str, sizeof(options_str),
                "sample_fmts=%s:sample_rates=%d:channel_layouts=%llu",
                av_get_sample_fmt_name( sv_output_format.a_fmt ),\
                sv_output_format.a_rate,
                sv_output_format.a_clayout );
        DLLOGE( "----> %s", options_str );
        if( ( tv_ret = avfilter_init_str( tp_ctx_format, options_str ) ) < 0 ){
            goto GT_configure_filtegraph_fail;
        }
    }
    if( ( tv_ret = avfilter_link( tp_output->filter_ctx,\
                    tp_output->pad_idx, tp_ctx_format, 0  ) ) < 0 ){
        goto GT_configure_filtegraph_fail;
    }
    tp_ctx_last = tp_ctx_format;

    // --------------------> dst
    if( !(tp_filt_dst = avfilter_get_by_name("abuffersink")) ){
        DLLOGE( "failed get resample!" );
        return AVERROR(EINVAL);
    }
    if( ( tp_ctx_dst = avfilter_graph_alloc_filter(\
                    tp_graph, tp_filt_dst, "dst") ) == NULL ){
        tv_ret = AVERROR(ENOMEM);
        goto GT_configure_filtegraph_fail;
    }
    if( ( tv_ret = avfilter_init_str( tp_ctx_dst, NULL ) ) < 0 ){
        goto GT_configure_filtegraph_fail;
    }
    if( ( tv_ret = avfilter_link( tp_ctx_last, 0, tp_ctx_dst, 0 )) < 0 )
        goto GT_configure_filtegraph_fail;
    tp_ctx_last = tp_ctx_dst;

    // --------------------> config the graph path
    if( ( tv_ret = avfilter_graph_config( tp_graph, NULL ) ) < 0 ){
        DLLOGE( "avfilter_graph_config failed : %d", tv_ret );
        goto GT_configure_filtegraph_fail;
    }

    //
    istream->a_filtergraph = tp_graph;
    istream->a_filtsrc = tp_ctx_src;
    istream->a_filtdst = tp_ctx_dst;
    return 0;
GT_configure_filtegraph_fail:
    if( tp_graph )
        avfilter_graph_free( &tp_graph );
    istream->a_filtergraph = NULL;
    istream->a_filtsrc = NULL;
    istream->a_filtdst = NULL;
    return tv_ret;
}
