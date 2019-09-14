#include <unistd.h>
#include <sys/stat.h>
#include <stropts.h>
#include <fcntl.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/soundcard.h>
#include <pthread.h>
#include <tinyalsa/asoundlib.h>

#include "audio_device.h"
#include "../tests/Debug.h"

// ----------------------------------------------------------------------------
// status
#define DC_AIN_STA_CMD_MASK             0xFFFF0000
#define DC_AIN_STA_CMD_OPENDEV          0x00010000
#define DC_AIN_STA_CMD_CLOSE            0x00020000
#define DC_AIN_STA_CMD_CLOSEALL         0x00030000
#define DC_AIN_STA_CMD_READ             0x00040000
#define DC_AIN_STA_MODEL_INIT           0x00000001
// device file type
#define DL_DEV_TYPE_CTRL            0x01
#define DL_DEV_TYPE_PLAY            0x02
#define DL_DEV_TYPE_CAPTURE         0x04
// alsa config
#define DL_AUD_ALSA_CARD            0
#define DL_AUD_ALSA_PERIOD_SIZE     1024
#define DL_AUD_ALSA_PERIOD_COUNT    8

// ----------------------------------------------------------------------------
typedef struct{
    e_au_dev         a_dev;
    unsigned         a_status;
#define DC_AUDIO_DEVICE_STA_RECORD          0x01
#define DC_AUDIO_DEVICE_STA_SPEECK          0x02
#define DC_AUDIO_DEVICE_STA_UPDATEFD_FD     0x04
#define DC_AUDIO_DEVICE_STA_INITED          0x03
#define DC_AUDIO_DEVICE_STA_STASH           0x40
    pthread_mutex_t  a_wmutex;
    pthread_mutex_t  a_rmutex;
    struct pcm      *a_wpcm;
    struct pcm      *a_rpcm;
    e_au_channel     a_rchannel;
    e_au_channel     a_wchannel;
    e_au_fmt         a_rfmt;
    e_au_fmt         a_wfmt;
    e_au_samplerate  a_rrate;
    e_au_samplerate  a_wrate;
    int              a_pvol;
    int              a_rvol;
}m_au_info;


// ----------------------------------------------------------------------------
/*     This function is used to generating device path according to input
 * param ( card, device amd type ). After generated the file path, it will
 * re-check is the path existed in file system. if exist then return 0;
 *
 *     'ifpbuf' is use to storage and return the file path, so you need
 * sure it have enough spaces.
 * */
static int sf_get_devices_name( char *ifpbuf, unsigned itype, int icard, int idevice );
static inline int silf_get_vol( m_au_info *iinfo, unsigned istatus );
static inline int silf_set_vol( m_au_info *iinfo, unsigned istatus );
static int sf_check_audio_format( unsigned istatus, unsigned int card,\
        unsigned int device, unsigned int channels, unsigned int rate,\
        unsigned int bits, unsigned int period_size, unsigned int period_count);
// ----------------------------------------------------------------------------
static int              sv_c0ctrl_fd = -1;
static m_au_info        sv_audev_info[4];
static unsigned         sv_status = 0;

// ----------------------------------------------------------------------------
#ifdef SUPPORT_LUA
#include "../../utils.h"

static int sf_lua_audio_device_init( lua_State *L, uv_loop_t *loop ){
    gf_audev_init();
    return 0;
}

static int sf_lua_audio_device_uninit( lua_State *L, uv_loop_t *loop ){
    gf_audev_uninit();
    return 0;
}

static int sf_lua_audio_device_unlink( lua_State *L, uv_loop_t *loop ){
    unsigned tv_args_sum = lua_gettop( L );

#if AUDIO_DEVICE_DEBUG
    DLLOGV("release all audio device!");
#endif
    gf_audev_release_devices( AUDIO_DEVICE_ALL );

    return 0;
}

static int sf_lua_audio_device_relink( lua_State *L, uv_loop_t *loop ){
#if AUDIO_DEVICE_DEBUG
    DLLOGV("recovery all audio device!");
#endif
    gf_audev_recovery_devices( AUDIO_DEVICE_ALL );
    return 0;
}

/*
@ Abstract:
@ param
    L
    loop

@ subparam:
    L1 : 
        e_au_dev
    L2 : 
        volume up delta

@ return
    R1:
        nil
        volume value
*/
int sf_lua_audio_device_volup( lua_State *L, uv_loop_t *loop ){
    int tv_dev;
    int tv_delta;
    int _i;

    if( lua_type( L, 1 ) == LUA_TNUMBER ){ tv_dev = lua_tonumber( L, 1 ); }
    else{ lua_pushnil( L ); return 1; }
    if( lua_type( L, 2 ) == LUA_TNUMBER ) tv_delta = lua_tonumber( L, 2 );
    else tv_delta = 10;

    for( _i = 0; _i < AUDIO_DEVICE_ALL; _i++ )
        if( tv_dev == sv_audev_info[ _i ].a_dev ){
            lua_pushnumber( L, gf_audev_speeck_volup( tv_dev, tv_delta ) );
        }
    if( _i == AUDIO_DEVICE_ALL ) lua_pushnil( L );
    return 1; 
}

int sf_lua_audio_device_voldown( lua_State *L, uv_loop_t *loop ){
    int tv_dev;
    int tv_delta;
    int _i;

    if( lua_type( L, 1 ) == LUA_TNUMBER ){ tv_dev = lua_tonumber( L, 1 ); }
    else{ lua_pushnil( L ); return 1; }
    if( lua_type( L, 2 ) == LUA_TNUMBER ) tv_delta = lua_tonumber( L, 2 );
    else tv_delta = 10;

    for( _i = 0; _i < AUDIO_DEVICE_ALL; _i++ )
        if( tv_dev == sv_audev_info[ _i ].a_dev ){
            lua_pushnumber( L, gf_audev_speeck_voldown( tv_dev, tv_delta ) );
        }
    if( _i == AUDIO_DEVICE_ALL ) lua_pushnil( L );
    return 1; 
}

void luv_hw_audev_init(lua_State *L, uv_loop_t *loop) {
    luv_register(L, loop, "hw_audev_init",          sf_lua_audio_device_init );
    luv_register(L, loop, "hw_audev_uninit",        sf_lua_audio_device_uninit );
    luv_register(L, loop, "hw_audev_unlink",        sf_lua_audio_device_unlink );
    luv_register(L, loop, "hw_audev_relink",        sf_lua_audio_device_relink );
    luv_register(L, loop, "hw_audev_volup",         sf_lua_audio_device_volup );
    luv_register(L, loop, "hw_audev_voldown",       sf_lua_audio_device_voldown );
}
#endif

void gf_audev_init( void ){
    char        tv_ctrlbuf[128];
    unsigned    i;

    if( sv_status & DC_AIN_STA_MODEL_INIT )
        return;

    memset( sv_audev_info, 0x00, sizeof( m_au_info )*4 );
    for( i=0; i<AUDIO_DEVICE_ALL; i++ ){
        sv_audev_info[ i ].a_dev = i;
        sv_audev_info[ i ].a_wpcm = NULL;
        sv_audev_info[ i ].a_rpcm = NULL;
        sv_audev_info[ i ].a_rvol = -1;
        pthread_mutex_init( &sv_audev_info[ i ].a_rmutex ,NULL );
        pthread_mutex_init( &sv_audev_info[ i ].a_wmutex ,NULL );
    }
    sv_status |= DC_AIN_STA_MODEL_INIT;
}

void gf_audev_uninit( void ){
    unsigned i;

    if( !( sv_status & DC_AIN_STA_MODEL_INIT ) )
        return;
    for( i=0; i<4; i++  ){
        pthread_mutex_lock( &sv_audev_info[i].a_wmutex );
        if( sv_audev_info[i].a_wpcm != NULL ){
            pcm_close( sv_audev_info[i].a_wpcm );
            sv_audev_info[i].a_wpcm = NULL;
        }
        pthread_mutex_unlock( &sv_audev_info[i].a_wmutex );
        pthread_mutex_destroy( &sv_audev_info[i].a_wmutex );
        // sv_audev_info[i].a_wmutex = {0};

        pthread_mutex_lock( &sv_audev_info[i].a_rmutex );
        if( sv_audev_info[i].a_rpcm != NULL ){
            pcm_close( sv_audev_info[i].a_rpcm );
            sv_audev_info[i].a_rpcm = NULL;
        }
        pthread_mutex_unlock( &sv_audev_info[i].a_rmutex );
        pthread_mutex_destroy( &sv_audev_info[i].a_rmutex );
        // sv_audev_info[i].a_rmutex  = {0};

        sv_audev_info[i].a_status = 0;
    }
    sv_status &= ~DC_AIN_STA_MODEL_INIT;
}

int gf_audev_config( m_au_conf *iconf ){
    struct pcm_config tv_config;
    m_au_info   *tp_info = &sv_audev_info[ iconf->a_num];
    unsigned     tv_status = iconf->a_status;
    int          tv_ioval    = 0;

    if( tv_status & DC_AUDEV_SET_SPEEK ){
        if( ( tp_info->a_wpcm != NULL )\
         && ( !( tv_status & DC_AUDEV_SET_FORCE ) ) ){
            // already init
            return -1;
        }
        if( tv_status & DC_AUDEV_SET_VOL ){
            if( tp_info->a_pvol != iconf->a_pvol ){
                tp_info->a_pvol = iconf->a_pvol;
                if( silf_set_vol( tp_info, DC_AUDEV_SET_SPEEK ) ){
                    return -1;
                }
            }
        }
        if( ( tv_status & DC_AUDEV_SET_CHANNEL )\
         || ( tv_status & DC_AUDEV_SET_RATE )\
         || ( tv_status & DC_AUDEV_SET_FMT ) ){
            pthread_mutex_lock( &tp_info->a_wmutex );

            if( tv_status & DC_AUDEV_SET_CHANNEL )
                tp_info->a_wchannel = iconf->a_channel;
            if( tv_status & DC_AUDEV_SET_FMT )
                tp_info->a_wfmt      = iconf->a_fmt;
            if( tv_status & DC_AUDEV_SET_RATE )
                tp_info->a_wrate     = iconf->a_rate;

            tv_config.channels = tp_info->a_wchannel;
            tv_config.rate     = tp_info->a_wrate;
            if( tp_info->a_wfmt == 16 )
                tv_config.format = PCM_FORMAT_S16_LE;
            else if( tp_info->a_wfmt == 32 )
                tv_config.format = PCM_FORMAT_S32_LE;
            tv_config.period_size = DL_AUD_ALSA_PERIOD_SIZE;
            tv_config.period_count = DL_AUD_ALSA_PERIOD_COUNT;
            tv_config.start_threshold = 0;
            tv_config.stop_threshold = 0;
            tv_config.silence_threshold = 0;

#if 0
            // param check
            if( !sf_check_audio_format( tv_status, DL_AUD_ALSA_CARD,\
                        DF_AU_ALSA_DEV(tp_info->a_dev), tv_config.channels,\
                        tv_config.rate, tp_info->a_wfmt, tv_config.period_size,\
                        tv_config.period_count )) {
                pthread_mutex_unlock( &tp_info->a_wmutex );
                return -1;
            }
#endif
            // update pcm
            if( tp_info->a_wpcm != NULL )
                pcm_close( tp_info->a_wpcm );
#if AUDIO_DEVICE_DEBUG
            DLLOGV("alsa w device: %d - %d", tp_info->a_dev, DF_AU_ALSA_DEV(tp_info->a_dev));
#endif
                if( ( tp_info->a_wpcm = pcm_open( DL_AUD_ALSA_CARD,\
                    DF_AU_ALSA_DEV(tp_info->a_dev), PCM_OUT, &tv_config) ) == NULL ){
                pthread_mutex_unlock( &tp_info->a_wmutex );
                return -1;
            }

            pthread_mutex_unlock( &tp_info->a_wmutex );
        }
        tp_info->a_status |= DC_AUDIO_DEVICE_STA_SPEECK;

    }else if( tv_status & DC_AUDEV_SET_RECORD ){
        if( ( tp_info->a_rpcm >= 0)\
         && ( !( tv_status & DC_AUDEV_SET_FORCE ) ) ){
            // already init
            return -1;
        }
        if( tv_status & DC_AUDEV_SET_VOL ){
            if( tp_info->a_rvol != iconf->a_rvol ){
                tp_info->a_rvol = iconf->a_rvol;
                if( silf_set_vol( tp_info, DC_AUDEV_SET_RECORD ) ){
                    return -1;
                }
            }
        }
        if( ( tv_status & DC_AUDEV_SET_CHANNEL )\
         || ( tv_status & DC_AUDEV_SET_RATE )\
         || ( tv_status & DC_AUDEV_SET_FMT ) ){
            pthread_mutex_lock( &tp_info->a_rmutex );

            if( tv_status & DC_AUDEV_SET_CHANNEL ){
                tp_info->a_rchannel = iconf->a_channel;
            }
            if( tv_status & DC_AUDEV_SET_FMT ){
                tp_info->a_rfmt      = iconf->a_fmt;
            }
            if( tv_status & DC_AUDEV_SET_RATE ){
                tp_info->a_rrate     = iconf->a_rate;
            }

            tv_config.channels = tp_info->a_rchannel;
            tv_config.rate     = tp_info->a_rrate;
            if( tp_info->a_rfmt == 32 )
                tv_config.format = PCM_FORMAT_S32_LE;
            else if( tp_info->a_rfmt == 16 )
                tv_config.format = PCM_FORMAT_S16_LE;
            tv_config.period_size = DL_AUD_ALSA_PERIOD_SIZE;
            tv_config.period_count = DL_AUD_ALSA_PERIOD_COUNT;
            tv_config.start_threshold = 0;
            tv_config.stop_threshold = 0;
            tv_config.silence_threshold = 0;

#if 0
            // param check
            if( !sf_check_audio_format( tv_status, DL_AUD_ALSA_CARD,\
                        DF_AU_ALSA_DEV( tp_info->a_dev ), tv_config.channels,\
                        tv_config.rate, tp_info->a_rfmt, tv_config.period_size,\
                        tv_config.period_count )) {
                pthread_mutex_unlock( &tp_info->a_rmutex );
                return -1;
            }
#endif
            // update pcm
            if( tp_info->a_rpcm != NULL )
                pcm_close( tp_info->a_rpcm );
#if AUDIO_DEVICE_DEBUG
            DLLOGV("alsa r device: %d - %d", tp_info->a_dev, DF_AU_ALSA_DEV(tp_info->a_dev));
#endif
            if( ( tp_info->a_rpcm = pcm_open( DL_AUD_ALSA_CARD,\
                    DF_AU_ALSA_DEV(tp_info->a_dev), PCM_IN, &tv_config) ) == NULL ){
#if AUDIO_DEVICE_DEBUG
            DLLOGV("alsa w device: %d - %d", tp_info->a_dev, DF_AU_ALSA_DEV(tp_info->a_dev));
#endif
                pthread_mutex_unlock( &tp_info->a_rmutex );
                return -1;
            }
            pthread_mutex_unlock( &tp_info->a_rmutex );
        }

        tp_info->a_status |= DC_AUDIO_DEVICE_STA_RECORD;
    }else{
        return -1;
    }

    return 0;
GT_gf_audev_config_clr_wfd:
#if AUDIO_DEVICE_DEBUG
    DLLOGE("config err!");
#endif
    if( tp_info->a_wpcm != NULL ){
        pthread_mutex_lock( &tp_info->a_wmutex );
        pcm_close( tp_info->a_wpcm );
        tp_info->a_wpcm = NULL;
        pthread_mutex_unlock( &tp_info->a_wmutex );
    }
    tp_info->a_status &= ~DC_AUDIO_DEVICE_STA_SPEECK;
    return -1;
GT_gf_audev_config_clr_rfd:
#if AUDIO_DEVICE_DEBUG
    DLLOGE("config err!");
#endif
     if( tp_info->a_rpcm != NULL  ){
        pthread_mutex_lock( &tp_info->a_rmutex );
        pcm_close( tp_info->a_rpcm );
        tp_info->a_rpcm = NULL;;
        pthread_mutex_unlock( &tp_info->a_rmutex );
    }
    tp_info->a_status &= ~DC_AUDIO_DEVICE_STA_RECORD;
    return -1;
}


// speech vol
int gf_audev_speeck_getvol( e_au_dev idev ){
    m_au_info       *tp_info = &sv_audev_info[idev];
    silf_get_vol( tp_info, DC_AUDEV_SET_SPEEK );
    return tp_info->a_pvol;
}

int gf_audev_speeck_setvol( e_au_dev idev, unsigned ivol ){
    m_au_info       *tp_info = &sv_audev_info[idev];
    tp_info->a_pvol = ivol;
    return silf_set_vol( tp_info, DC_AUDEV_SET_SPEEK );
}

int gf_audev_speeck_volup( e_au_dev idev, unsigned idelta ){
    m_au_info       *tp_info = &sv_audev_info[idev];
    tp_info->a_pvol = ((tp_info->a_pvol + idelta)>100) ? 100 : tp_info->a_pvol+idelta;
    return silf_set_vol( tp_info, DC_AUDEV_SET_SPEEK );
}

int gf_audev_speeck_voldown( e_au_dev idev, unsigned idelta ){
    m_au_info       *tp_info = &sv_audev_info[idev];
    tp_info->a_pvol = ((tp_info->a_pvol - idelta)<0) ? 0 : tp_info->a_pvol+idelta;
    return silf_set_vol( tp_info, DC_AUDEV_SET_SPEEK );
}

int gf_audev_block_play( e_au_dev idev, char *ibuf, unsigned ilen  ){
    m_au_info   *tp_info = &sv_audev_info[ idev ];
    int          tv_ret;

    pthread_mutex_lock( &tp_info->a_wmutex );
    if( tp_info->a_wpcm == NULL ){
        // The target devices didnt init or something error happened.
        tv_ret = -1;
        goto GT_gf_audev_block_play_ret;
    }

    tv_ret = pcm_write( tp_info->a_wpcm, ibuf, ilen );
    // DLLOGV("gf_audev_block_play : write : fd %d ret : %d", tv_fd ,tv_ret );
    if( tv_ret < 0 ){
        pcm_close(  tp_info->a_wpcm );
        tp_info->a_wpcm = NULL;
        tp_info->a_status &= ~DC_AUDIO_DEVICE_STA_SPEECK;
        tv_ret = -1;
        goto GT_gf_audev_block_play_ret;
    }
    tv_ret = ilen;

GT_gf_audev_block_play_ret:
    pthread_mutex_unlock( &tp_info->a_wmutex );
    return tv_ret;
}



int gf_audev_block_record( e_au_dev idev, void *ibuf, unsigned ilen  ){
    m_au_info   *tp_info = &sv_audev_info[ idev ];
    int          tv_ret = 0;
    char        *tp_rbuf = ibuf;

    pthread_mutex_lock( &tp_info->a_rmutex );
    if( tp_info->a_rpcm == NULL ){
        // The target devices didnt init or something error happened.
        tv_ret = -1;
        goto GT_gf_audev_block_record_ret;
    }

    tv_ret = pcm_read( tp_info->a_rpcm, tp_rbuf, ilen );
#if AUDIO_DEVICE_DEBUG
        // DLLOGD("audev rfd:%d, ret:%d, readlen:%d ", tp_info->a_rpcm, tv_ret, ilen);
#endif
    if( tv_ret < 0 ){
        pcm_close( tp_info->a_rpcm );
        tp_info->a_rpcm = NULL;
        tp_info->a_status &= ~DC_AUDIO_DEVICE_STA_RECORD;
        tv_ret = -1;
        goto GT_gf_audev_block_record_ret;
     }

    tv_ret = ilen;
GT_gf_audev_block_record_ret:
    pthread_mutex_unlock( &tp_info->a_rmutex );
    return tv_ret;
}



int gf_audev_release_devices( e_au_dev idev ){
    m_au_info   *tp_info;
    int          tv_ret;

    if( idev == AUDIO_DEVICE_ALL ){
        unsigned _i;
        for( _i = 0; _i < AUDIO_DEVICE_ALL; _i++ ){
            tp_info = &sv_audev_info[ _i ];
            if( ( tp_info->a_wpcm == NULL )&&( tp_info->a_rpcm == NULL ) )
                continue;
            pthread_mutex_lock( &tp_info->a_wmutex );
            if(  tp_info->a_wpcm != NULL )
#if AUDIO_DEVICE_DEBUG
                DLLOGV("close dsp ret: %d", pcm_close(  tp_info->a_wpcm ) );
#else
                pcm_close(  tp_info->a_wpcm );
#endif
            tp_info->a_wpcm = NULL;
            pthread_mutex_unlock( &tp_info->a_wmutex );
            pthread_mutex_lock( &tp_info->a_rmutex );
            if( tp_info->a_rpcm != NULL )
#if AUDIO_DEVICE_DEBUG
                DLLOGV("close dsp ret: %d", pcm_close( tp_info->a_rpcm ) );
#else
                pcm_close( tp_info->a_rpcm);
#endif
            tp_info->a_rpcm = NULL;
            pthread_mutex_unlock( &tp_info->a_rmutex );
            tp_info->a_status |= DC_AUDIO_DEVICE_STA_STASH;
        }
    }else{
        tp_info = &sv_audev_info[ idev ];
        if( ( tp_info->a_wpcm == NULL )&&( tp_info->a_rpcm == NULL ) ){
            tv_ret = -1;
            goto GT_gf_audev_release_devices_ret;
        }
        pthread_mutex_lock( &tp_info->a_wmutex );
        if(  tp_info->a_wpcm != NULL )
#if AUDIO_DEVICE_DEBUG
            DLLOGV("close dsp ret: %d", pcm_close( tp_info->a_wpcm ) );
#else
            pcm_close( tp_info->a_wpcm );
#endif
        tp_info->a_wpcm = NULL;
        pthread_mutex_unlock( &tp_info->a_wmutex );
        pthread_mutex_lock( &tp_info->a_rmutex );
        if( tp_info->a_rpcm != NULL )
#if AUDIO_DEVICE_DEBUG
            DLLOGV("close dsp ret: %d", pcm_close( tp_info->a_rpcm ) );
#else
            pcm_close( tp_info->a_rpcm );
#endif
        tp_info->a_rpcm = NULL;
        pthread_mutex_unlock( &tp_info->a_rmutex );
        tp_info->a_status |= DC_AUDIO_DEVICE_STA_STASH;
    }

    tv_ret = 0;
GT_gf_audev_release_devices_ret:
    return tv_ret;
}



int gf_audev_recovery_devices( e_au_dev idev ){
    m_au_info   *tp_info;
    m_au_conf    tv_conf;
    int          tv_ret;
    unsigned     tv_status;
    unsigned     _i;
    unsigned     i_base;
    unsigned     i_over;

    if( idev == AUDIO_DEVICE_ALL ){
        i_base = 0;
        i_over = AUDIO_DEVICE_ALL;
    }else{
        i_base = idev;
        i_over = idev + 1;
    }

    for( _i = i_base; _i < i_over; _i++ ){
        tp_info = &sv_audev_info[ _i ];
        if( ( tp_info->a_wpcm == NULL )&&( tp_info->a_rpcm == NULL ) )
            continue;
        tv_status = tp_info->a_status;
        if( tv_status & DC_AUDIO_DEVICE_STA_SPEECK ){
            tv_conf.a_status = 0;
            tv_conf.a_status |= DC_AUDEV_SET_FORCE;
            tv_conf.a_status |= DC_AUDEV_SET_SPEEK;
            tv_conf.a_status |= DC_AUDEV_SET_VOL;
            tv_conf.a_status |= DC_AUDEV_SET_CHANNEL;
            tv_conf.a_status |= DC_AUDEV_SET_FMT;
            tv_conf.a_status |= DC_AUDEV_SET_RATE;
            tv_conf.a_num       = _i;
            tv_conf.a_channel   = tp_info->a_wchannel;
            tv_conf.a_fmt       = tp_info->a_wfmt;
            tv_conf.a_rate      = tp_info->a_wrate;
            tv_conf.a_pvol      = tp_info->a_pvol;

            tp_info->a_wchannel = 0;
            tp_info->a_wfmt = 0;
            tp_info->a_wrate =0;
            tp_info->a_pvol = -1;

            tp_info->a_status &= ~DC_AUDIO_DEVICE_STA_SPEECK;
            if( gf_audev_config( &tv_conf ) )
                ;
            // warning: _i device init failed
        }

        if( tv_status & DC_AUDIO_DEVICE_STA_RECORD ){
            tv_conf.a_status = 0;
            tv_conf.a_status |= DC_AUDEV_SET_FORCE;
            tv_conf.a_status |= DC_AUDEV_SET_RECORD;
            tv_conf.a_status |= DC_AUDEV_SET_VOL;
            tv_conf.a_status |= DC_AUDEV_SET_CHANNEL;
            tv_conf.a_status |= DC_AUDEV_SET_FMT;
            tv_conf.a_status |= DC_AUDEV_SET_RATE;
            tv_conf.a_num       = _i;
            tv_conf.a_channel   = tp_info->a_rchannel;
            tv_conf.a_fmt       = tp_info->a_rfmt;
            tv_conf.a_rate      = tp_info->a_rrate;
            tv_conf.a_pvol      = tp_info->a_rvol;

            tp_info->a_rchannel = 0;
            tp_info->a_rfmt = 0;
            tp_info->a_rrate =0;
            tp_info->a_rvol = -1;

            tp_info->a_status &= ~DC_AUDIO_DEVICE_STA_RECORD;
            if( gf_audev_config( &tv_conf ) )
                ;
            // warning: _i device init failed
        }
    }
    tv_ret = 0;
GT_gf_audev_recovery_devices_ret:
    return tv_ret;
}




// ---------------------------------------------------------------------------
static inline int silf_get_vol( m_au_info *iinfo, unsigned istatus ){
    struct mixer       *tp_mixer;
    struct mixer_ctl   *tp_ctl;
    int                 tv_vol = iinfo->a_pvol;

    if( ( tp_mixer = mixer_open( DL_AUD_ALSA_CARD ) ) == NULL ){
#if AUDIO_DEVICE_DEBUG
        DLLOGE("audio.device.alsa.ctrl_device:failed open!");
#endif
        return -1;
    }
    // get handle
    tp_ctl = mixer_get_ctl( tp_mixer, 0 ); // control == 0 == set/get vol
    // set value
    if( istatus & DC_AUDEV_SET_SPEEK ){
        tv_vol = mixer_ctl_get_value( tp_ctl, 0 );
        tv_vol = tv_vol*10/6;
        iinfo->a_pvol = tv_vol;
    }else if( istatus & DC_AUDEV_SET_RECORD ) {
        tv_vol = mixer_ctl_get_value( tp_ctl, 1 );
        tv_vol = tv_vol*10/6;
        iinfo->a_pvol = tv_vol;
    }
#if AUDIO_DEVICE_DEBUG
    DLLOGV("audio.device.alsa: dev->%d pvol->%d rvol->%d",\
            iinfo->a_dev, iinfo->a_pvol, iinfo->a_rvol );
#endif
    mixer_close( tp_mixer );
    return 0;
}

static inline int silf_set_vol( m_au_info *iinfo, unsigned istatus ){
    struct mixer       *tp_mixer;
    struct mixer_ctl   *tp_ctl;
    int                 tv_vol;

    if( istatus & DC_AUDEV_SET_SPEEK ){
        if( ( tp_mixer = mixer_open( DL_AUD_ALSA_CARD ) ) == NULL ){
#if AUDIO_DEVICE_DEBUG
            DLLOGE("audio.device.alsa.ctrl_device:failed open!");
#endif
            return -1;
        }
        // get handle
        tp_ctl = mixer_get_ctl( tp_mixer, 0 ); // control == 0 == set/get vol
        // set value
        tv_vol = iinfo->a_pvol;
        if( tv_vol < 0 )
            tv_vol = 0;
        else if( tv_vol > 100 )
            tv_vol = 60;
        else
            tv_vol = tv_vol*6/10;
        mixer_ctl_set_value( tp_ctl, 0, tv_vol );
        mixer_ctl_set_value( tp_ctl, 1, tv_vol );
#if AUDIO_DEVICE_DEBUG
        DLLOGV("audio.device.alsa: dev->%d pvol->%d rvol->%d",\
                iinfo->a_dev,\
                mixer_ctl_get_value( tp_ctl, 0 ),\
                mixer_ctl_get_value( tp_ctl, 1 ));
#endif
        mixer_close( tp_mixer );
    }else if( istatus & DC_AUDEV_SET_RECORD ) {
    }
    // scale the value
    return 0;
}

static int sf_get_devices_name( char *ifpbuf, unsigned itype, int icard, int idevice ){
    switch( itype ){
    case DL_DEV_TYPE_CTRL:
        ifpbuf[ sprintf( ifpbuf, "/dev/snd/controlC%d", icard ) ] = '\0';
    break;
    case DL_DEV_TYPE_PLAY:
        ifpbuf[ sprintf( ifpbuf, "/dev/snd/controlC%d", icard ) ] = '\0';
    break;
    case DL_DEV_TYPE_CAPTURE:
        ifpbuf[ sprintf( ifpbuf, "/dev/snd/controlC%d", icard ) ] = '\0';
    break;
    default:
        return -1;
    break;
    }
#if AUDIO_DEVICE_DEBUG
    DLLOGV( "audio.device.get_device_path: %s", ifpbuf );
#endif
    if( access( ifpbuf, F_OK ) ){
        return -1;
    }else{
        return 0;
    }
}
static int check_param(struct pcm_params *params, unsigned int param,\
        unsigned int value, char *param_name, char *param_unit){
    unsigned int min;
    unsigned int max;
    int is_within_bounds = 1;

    min = pcm_params_get_min(params, param);
    if (value < min) {
        fprintf(stderr, "%s is %u%s, device only supports >= %u%s\n", param_name, value,
                param_unit, min, param_unit);
        is_within_bounds = 0;
    }

    max = pcm_params_get_max(params, param);
    if (value > max) {
        fprintf(stderr, "%s is %u%s, device only supports <= %u%s\n", param_name, value,
                param_unit, max, param_unit);
        is_within_bounds = 0;
    }

    return is_within_bounds;
}
static int sf_check_audio_format( unsigned istatus, unsigned int card,\
        unsigned int device, unsigned int channels, unsigned int rate,\
        unsigned int bits, unsigned int period_size, unsigned int period_count){
    struct pcm_params *params;
    int can_play;

    if( istatus & DC_AUDEV_SET_SPEEK )
        params = pcm_params_get( card, device, PCM_OUT );
    if( istatus & DC_AUDEV_SET_RECORD )
        params = pcm_params_get( card, device, PCM_IN );
    if (params == NULL) {
        fprintf(stderr, "Unable to open PCM device %u.\n", device);
        return 0;
    }

    can_play = check_param(params, PCM_PARAM_RATE, rate, "Sample rate", "Hz");
    can_play &= check_param(params, PCM_PARAM_CHANNELS, channels, "Sample", " channels");
    can_play &= check_param(params, PCM_PARAM_SAMPLE_BITS, bits, "Bitrate", " bits");
    can_play &= check_param(params, PCM_PARAM_PERIOD_SIZE, period_size, "Period size", "Hz");
    can_play &= check_param(params, PCM_PARAM_PERIODS, period_count, "Period count", "Hz");

    pcm_params_free(params);

    return can_play;
}

