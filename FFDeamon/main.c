#include <libavformat/avformat.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include "../tools/params.h"
#include "../tools/tools.h"
#include "../tools/Debug.h"
#include "globalparameters.h"
#include "devices/audio_device.h"
#include "msgCenter.h"
#include "task.h"
#include "player.h"

// -----------------------------------------------------------------------------
typedef struct{
    m_params    oss;
    m_params    oss_name;
    m_params    oss_vol;
    m_params    alsa;
    m_params    uds;
    m_params    ochannel;
    m_params    ofmt;
    m_params    orate;
    m_params    list_fmt;
}m_inputparams;



// -----------------------------------------------------------------------------
static int AVDeamon_resgite_all_device( void );
static int main_active_main_thread( void );
static int kill_other_deamon( void );
static int sf_init_output_device( void );
// static int AVDeamon_register_local_devices( void );

// -----------------------------------------------------------------------------
// global
extern AVOutputFormat       ff_oss_muxer;
// extern AVOutputFormat       ff_uds_muxer;
// local
static pthread_cond_t   sv_cond;
static pthread_mutex_t   sv_mutex;

static m_inputparams gv_params = {
    .oss =  { .pattern = "--oss",       .subpattern = NULL,     .type = E_PARAMS_TYPE_BOOL,      .data.abool = G_OUTPUT_ENABLE_OSS, },
    .oss_name =  { .pattern = "--oss_name",       .subpattern = NULL,     .type = E_PARAMS_TYPE_STR,      .data.astr = G_OUTPUT_OSS_NAME, },
    .oss_vol =  { .pattern = "--oss_vol",       .subpattern = NULL,     .type = E_PARAMS_TYPE_INT,      .data.aint = G_OUTPUT_OSS_PVOL, },
    .alsa =  { .pattern = "--alsa",     .subpattern = NULL,     .type = E_PARAMS_TYPE_BOOL,      .data.abool = G_OUTPUT_ENABLE_ALSA, },
    .uds =  { .pattern = "--uds",       .subpattern = NULL,     .type = E_PARAMS_TYPE_BOOL,      .data.abool = G_OUTPUT_ENABLE_UDS, },
    .ochannel =  { .pattern = "--output_channel",       .subpattern = "-oac",     .type = E_PARAMS_TYPE_INT,      .data.aint = G_OUTPUT_CHANNEL, },
    .ofmt =  { .pattern = "--output_fmt",       .subpattern = "-oaf",     .type = E_PARAMS_TYPE_STR,      .data.astr = G_OUTPUT_FORMAT, },
    .orate =  { .pattern = "--output_rate",       .subpattern = "-oar",     .type = E_PARAMS_TYPE_INT,      .data.aint = G_OUTPUT_RATE, },
    .list_fmt =  { .pattern = "--list-fmt",       .subpattern = NULL,     .type = E_PARAMS_TYPE_BOOL,      .data.abool = PFLASE, },
};


// -----------------------------------------------------------------------------
int main( int argc, char *argv[] ){
    DLLOGD("Hello world!");

    // kill others deamon
    kill_other_deamon();

    // get env param
    if( param_get( argc, argv, (m_params*)&gv_params, sizeof( m_inputparams )/sizeof( m_params ) ) )
        return -1;
    param_show( (m_params*)&gv_params, sizeof( m_inputparams )/sizeof( m_params ) );
    if( gv_params.list_fmt.data.abool ){
        // utils_show_audio_format();
        return 0;
    }

    AVDeamon_resgite_all_device();

    player_init( 2, av_get_default_channel_layout( gv_params.ochannel.data.aint ),\
            av_get_sample_fmt( gv_params.ofmt.data.astr ), gv_params.orate.data.aint );

    player_open_output_device( gv_params.oss.data.abool,\
           gv_params.alsa.data.abool, gv_params.uds.data.abool );

    // initialize output device
    if( sf_init_output_device() ){
        return -1;
    }

    pthread_cond_init( &sv_cond, NULL );
    pthread_mutex_init( &sv_mutex, NULL );

    task_init( main_active_main_thread );
    msgCenterInit();

    while( 1 ){
        mTaskNode      *tp_node = NULL;

        while( ( tp_node = task_list_pop( ) ) ){
            switch( tp_node->a_action ){
            case EMsgActionTone:{
                DLLOGI( "path : %s", tp_node->a_url.a_path );
                player_play( G_PLAYER_STREAM_TONE, tp_node->a_url.a_path );

            }break;
            case EMsgActionPlay:{
                DLLOGI( "path : %s", tp_node->a_url.a_path );
                int tv_fd;
                char tv_aubuf[3200];
                int tv_rlen;

                tv_fd = open( "/tmp/happy_48k_1ch_30s.mp3", O_RDONLY );
                DLLOGI( "open happy_48k_1ch_30s.mp3 : %d", tv_fd );
                // init oss
                //------------------------------------------------------------------
                avdevice_register_all();
                DLLOGI( "avdevice : %s", avdevice_configuration() );
                //------------------------------------------------------------------
                while(1){
                    tv_rlen = read( tv_fd, tv_aubuf, 3200 );
                    DLLOGI( "read size : %d", tv_rlen );
                    if( tv_rlen <= 0 )
                        break;
                    // write to oss
                }
            }break;
            case EMsgActionInsert:{
            
            }break;
            case EMsgActionStop:{
            
            }break;
            case EMsgActionPause:{
            
            }break;
            case EMsgActionResume:{
            
            }break;
            case EMsgActionChangeOutput:{
            
            }break;
            default:break;
            }
            if( tp_node )
                task_destory_ctrlpoint( tp_node );
            tp_node = NULL;
        }
        pthread_cond_wait( &sv_cond, &sv_mutex );
    }

    return 0;
}

// -----------------------------------------------------------------------------
static int AVDeamon_resgite_all_device( void ){
    av_register_all();
    avformat_network_init();
    avcodec_register_all();
    avdevice_register_all();
    avfilter_register_all();
    // AVDeamon_register_local_devices();
    return 0;
}

static int main_active_main_thread( void ){
    pthread_cond_broadcast( &sv_cond );
    return 0;
}


static int kill_other_deamon( void ){
    int     tv_pid;
    int     tv_bropid[ 10 ];
    int     tv_bronum;
    char    tv_cmd[128];
    int     i;

    tv_pid = getpid();
    // dllogv( "%s pid : %d", argv[0], tv_pid );
    tv_bronum = get_exec_pid( "AVDEAMON", tv_bropid, 10 );
    for( i=0; i<tv_bronum; i++ ){
        if( tv_bropid[ i ] == tv_pid )
            continue;
        tv_cmd[ sprintf( tv_cmd, "kill -9 %d", tv_bropid[i] ) ] = '\0';
        DLLOGV( "cmd : %s", tv_cmd );
        system( tv_cmd );
        // dllogv( "pid : %d", tv_bropid[ i ] );
    }
    return 0;
}

static int sf_init_output_device( void ){
    if( gv_params.oss.data.abool ){
        DLLOGD( "init oss!" );
        m_au_conf tv_conf;

        tv_conf.a_status = 0;
        tv_conf.a_status |= DC_AUDEV_SET_VOL;
        tv_conf.a_status |= DC_AUDEV_SET_CHANNEL;
        tv_conf.a_status |= DC_AUDEV_SET_FMT;
        tv_conf.a_status |= DC_AUDEV_SET_RATE;
        tv_conf.a_status |= DC_AUDEV_SET_FRAGMENT;
        tv_conf.a_status |= DC_AUDEV_SET_SPEEK;
        tv_conf.a_status |= DC_AUDEV_SET_FORCE;
        tv_conf.a_num = gf_audev_get_devid_from_name(\
                gv_params.oss_name.data.astr );
        tv_conf.a_channel   = AUDIO_CHANNEL_1;
        tv_conf.a_fmt       = AUDIO_FMT_S16_LE;
        tv_conf.a_rate      = AUDIO_SAMPLE_RATE_48000;
        tv_conf.a_pvol      = gv_params.oss_vol.data.aint;
        // cent 2 16
        // size 2^4 - 2^13 ( 16 8k )
        tv_conf.a_fragment = 12|(8<<16); // 2^12 + 8
        // tv_conf.a_fragment = 13|(8<<16); // 2^10 + 8
        while( gf_audev_config( &tv_conf ) ){
            DLLOGW("Retry OSS device open");
            sleep( 1 );
        }
    }
    if( gv_params.uds.data.abool ){
        DLLOGD( "init uds!" );
        return -1;
    }
    if( gv_params.alsa.data.abool ){
        DLLOGD( "init alsa!" );
        return -1;
    }
    return 0;
}

/*
const AVIOInterruptCB int_cb = { decode_interrupt_cb, NULL };
static int play_oss(void){
    AVFormatContext     *tp_format_context;
    AVOutputFormat      *tp_oformat;
    OutputStream        *tp_ost;
    AVStream            *tp_st;
    AVDictionary        *tp_dictionary;

    if( ( tp_format_context = avformat_alloc_context() ) == NULL ){
        DLLOGE( "%d", __LINE__ );
        return -1;
    }

    tp_oformat = av_guess_format( "oss", NULL, NULL);

    oc->oformat = file_oformat;
    oc->interrupt_callback = int_cb;
    av_strlcpy(oc->filename, "/dev/dsp", sizeof("/dev/dsp"));

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

    {
        AVPacket tv_pkg;
        file_oformat->write_packet( oc, tp_pkg );
    }
    file_oformat->write_trailer( oc,  ); 

}

static int AVDeamon_register_local_devices( void ){
    av_register_output_format( &ff_oss_muxer );
    // av_register_output_format( &ff_uds_muxer );
    return 0;
}
*/

