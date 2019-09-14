#include <unistd.h>
#include <sys/stat.h>
#include <stropts.h>
#include <fcntl.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/soundcard.h>
#include <pthread.h>

#include "audio_device.h"
#include "../../tools/Debug.h"

// ----------------------------------------------------------------------------
// status
#define DC_AIN_STA_CMD_MASK             0xFFFF0000
#define DC_AIN_STA_CMD_OPENDEV          0x00010000
#define DC_AIN_STA_CMD_CLOSE            0x00020000
#define DC_AIN_STA_CMD_CLOSEALL         0x00030000
#define DC_AIN_STA_CMD_READ             0x00040000
#define DC_AIN_STA_MODEL_INIT           0x00000001
// reconnect
#define DC_RECONNECT_INTERVAL           500000 // us
#define DC_RECONNECT_TIMES              10


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
    int              a_rfd;
    int              a_wfd;
    e_au_channel     a_rchannel;
    e_au_channel     a_wchannel;
    e_au_fmt         a_rfmt;
    e_au_fmt         a_wfmt;
    e_au_samplerate  a_rrate;
    e_au_samplerate  a_wrate;
    unsigned         a_pvol;
    unsigned         a_rvol;
    int              a_wfragment;
}m_au_info;


// ----------------------------------------------------------------------------
static inline int silf_set_speeck_vol( m_au_info *iinfo );
static inline int silf_get_speeck_vol( m_au_info *iinfo );
static inline int silf_set_record_vol( m_au_info *iinfo );
static inline int silf_set_update_dsp_handle( m_au_info *iinfo, unsigned istatus );

// ----------------------------------------------------------------------------
static m_au_info        sv_audev_info[4];
static unsigned         sv_status = 0;
#if AUDIO_DEVICE_REC_PLAY
static int              sv_recp_fd = -1;
#endif

// ----------------------------------------------------------------------------
void gf_audev_init( void ){
    unsigned i;

    if( sv_status & DC_AIN_STA_MODEL_INIT )
        return;
    memset( sv_audev_info, 0x00, sizeof( m_au_info )*4 );
    for( i=0; i<AUDIO_DEVICE_ALL; i++ ){
        sv_audev_info[ i ].a_dev = i;
        // sv_audev_info[ i ].a_fd = -1;
        sv_audev_info[ i ].a_rfd = -1;
        sv_audev_info[ i ].a_wfd = -1;
        sv_audev_info[ i ].a_pvol = -1;
        sv_audev_info[ i ].a_rvol = -1;
        pthread_mutex_init( &sv_audev_info[ i ].a_rmutex ,NULL );
        pthread_mutex_init( &sv_audev_info[ i ].a_wmutex ,NULL );
    }
    sv_status |= DC_AIN_STA_MODEL_INIT;
#if AUDIO_DEVICE_REC_PLAY
    if( sv_recp_fd <= 0 ){
        sv_recp_fd = open( "/tmp/record_play.pcm", O_RDWR|O_CREAT|O_TRUNC, S_IRWXU|S_IRWXG|S_IRWXO );
    }
#endif
}

void gf_audev_uninit( void ){
    unsigned i;

    if( !( sv_status & DC_AIN_STA_MODEL_INIT ) )
        return;
    for( i=0; i<4; i++  ){
        pthread_mutex_lock( &sv_audev_info[i].a_wmutex );
        if( sv_audev_info[i].a_wfd >= 0 ){
            close( sv_audev_info[i].a_wfd );
            sv_audev_info[i].a_wfd = -1;
        }
        pthread_mutex_unlock( &sv_audev_info[i].a_wmutex );

        pthread_mutex_lock( &sv_audev_info[i].a_rmutex );
        if( sv_audev_info[i].a_rfd >= 0 ){
            close( sv_audev_info[i].a_rfd );
            sv_audev_info[i].a_rfd = -1;
        }
        pthread_mutex_unlock( &sv_audev_info[i].a_rmutex );

        sv_audev_info[i].a_status = 0;
    }
    sv_status &= ~DC_AIN_STA_MODEL_INIT;
}

int gf_audev_config( m_au_conf *iconf ){
    m_au_info   *tp_info = &sv_audev_info[ iconf->a_num];
    unsigned     tv_status = iconf->a_status;
    int          tv_ioval    = 0;

    if( sv_status & DC_AIN_STA_MODEL_INIT )
        gf_audev_init();

    if( tv_status & DC_AUDEV_SET_SPEEK ){
        if( ( tp_info->a_wfd >= 0)\
         && ( !( tv_status & DC_AUDEV_SET_FORCE ) ) ){
            // already init
            goto GT_gf_audev_config_err;
        }else{
            // already init but covery histroy
            ;
        }
        if( tv_status & DC_AUDEV_SET_VOL ){
            if( tp_info->a_pvol != iconf->a_pvol ){
                tp_info->a_pvol = iconf->a_pvol;
                if( silf_set_speeck_vol( tp_info ) ){
                    goto GT_gf_audev_config_err;
                }
            }
        }
        if( ( tv_status & DC_AUDEV_SET_CHANNEL )\
         || ( tv_status & DC_AUDEV_SET_RATE )\
         || ( tv_status & DC_AUDEV_SET_FMT )\
         || ( tv_status & DC_AUDEV_SET_FRAGMENT ) ){
             if( tp_info->a_wfd < 0 ){
                pthread_mutex_lock( &tp_info->a_wmutex );
                if( silf_set_update_dsp_handle( tp_info, tv_status ) ){
                    pthread_mutex_unlock( &tp_info->a_wmutex );
                    goto GT_gf_audev_config_err;
                }
                pthread_mutex_unlock( &tp_info->a_wmutex );
            }
        }

        if( tv_status & DC_AUDEV_SET_CHANNEL ){
            if( tp_info->a_wchannel != iconf->a_channel ){
                tv_ioval = iconf->a_channel;
                if( ioctl( tp_info->a_wfd, SNDCTL_DSP_CHANNELS, &tv_ioval ) == -1 ){
                    goto GT_gf_audev_config_clr_wfd;
                }
                if( tv_ioval != (int)(iconf->a_channel) ){
#if AUDIO_DEVICE_DEBUG
                    DLLOGE("audio.config.%s.speech.channel:err(%d-%d)!",\
                            DF_AU_DSP_NAME( tp_info->a_dev ), iconf->a_channel, tv_ioval );
#endif
                    goto GT_gf_audev_config_clr_wfd;
                }
                tp_info->a_wchannel = iconf->a_channel;
            }
        }
        if( tv_status & DC_AUDEV_SET_FMT ){
            if( tp_info->a_wfmt != iconf->a_fmt ){
                tv_ioval = iconf->a_fmt;
                if( ioctl( tp_info->a_wfd, SNDCTL_DSP_SETFMT, &tv_ioval) < 0 ){
                    // write fmt failed!
                    goto GT_gf_audev_config_clr_wfd;
                }
                if( tv_ioval != (int)(iconf->a_fmt) ){
#if AUDIO_DEVICE_DEBUG
                    DLLOGE("audio.config.%s.speech.fmt:err(%d-%d)!",\
                            DF_AU_DSP_NAME( tp_info->a_dev ), iconf->a_fmt, tv_ioval );
#endif
                    goto GT_gf_audev_config_clr_wfd;
                }
                tp_info->a_wfmt      = iconf->a_fmt;
            }
        }
        if( tv_status & DC_AUDEV_SET_RATE ){
            if( tp_info->a_wrate != iconf->a_rate ){
                tv_ioval = iconf->a_rate;
                if( ioctl( tp_info->a_wfd, SNDCTL_DSP_SPEED, &tv_ioval ) == -1 ){
                    goto GT_gf_audev_config_clr_wfd;
                }
                if( tv_ioval != (int)(iconf->a_rate) ){
#if AUDIO_DEVICE_DEBUG
                    DLLOGE("audio.config.%s.speech.rate:err(%d-%d)!",\
                            DF_AU_DSP_NAME( tp_info->a_dev ), iconf->a_rate, tv_ioval );
#endif
                    goto GT_gf_audev_config_clr_wfd;
                }
                tp_info->a_wrate     = iconf->a_rate;
            }
        }

        if( tv_status & DC_AUDEV_SET_FRAGMENT ){
            if( tp_info->a_wfragment != iconf->a_fragment ){
                tv_ioval = iconf->a_fragment;
                if( ioctl( tp_info->a_wfd, SNDCTL_DSP_SETFRAGMENT, &tv_ioval ) == -1 ){
                    goto GT_gf_audev_config_clr_wfd;
                }
                if( tv_ioval != iconf->a_fragment ){
#if AUDIO_DEVICE_DEBUG
                    DLLOGE("audio.config.%s.speech.fragment:err(%d-%d)!",\
                            DF_AU_DSP_NAME( tp_info->a_dev ), iconf->a_fragment, tv_ioval );
#endif
                    goto GT_gf_audev_config_clr_wfd;
                }
                tp_info->a_wfragment     = iconf->a_fragment;
            }
        }

        tp_info->a_status |= DC_AUDIO_DEVICE_STA_SPEECK;
#if AUDIO_DEVICE_DEBUG
    DLLOGI("audio.config.%s.speeck:success!", DF_AU_DSP_NAME( tp_info->a_dev ));
#endif
    }else if( iconf->a_status & DC_AUDEV_SET_RECORD ){
        if( ( tp_info->a_rfd >= 0)\
         && ( !( iconf->a_status & DC_AUDEV_SET_FORCE ) ) ){
            // already init
            goto GT_gf_audev_config_err;
        }else{
            // already init but covery histroy
            ;
        }
        if( tv_status & DC_AUDEV_SET_VOL ){
            if( tp_info->a_rvol != iconf->a_rvol ){
                tp_info->a_rvol = iconf->a_rvol;
                if( silf_set_record_vol( tp_info ) ){
                    goto GT_gf_audev_config_err;
                }
            }
        }
        if( ( tv_status & DC_AUDEV_SET_CHANNEL )\
         || ( tv_status & DC_AUDEV_SET_RATE )\
         || ( tv_status & DC_AUDEV_SET_FMT ) ){
             if( tp_info->a_rfd < 0 ){
                pthread_mutex_lock( &tp_info->a_rmutex );
                if( silf_set_update_dsp_handle( tp_info, tv_status ) ){
                    pthread_mutex_unlock( &tp_info->a_rmutex );
                    goto GT_gf_audev_config_err;
                }
                pthread_mutex_unlock( &tp_info->a_rmutex );
            }
        }

        if( tv_status & DC_AUDEV_SET_CHANNEL ){
            if( tp_info->a_rchannel != iconf->a_channel ){
                tv_ioval = iconf->a_channel;
                if( ioctl( tp_info->a_rfd, SNDCTL_DSP_CHANNELS, &tv_ioval ) == -1 ){
                    goto GT_gf_audev_config_clr_rfd;
                }
                if( tv_ioval != (int)(iconf->a_channel) ){
#if AUDIO_DEVICE_DEBUG
                    DLLOGE("audio.config.%s.record.channel:err(%d-%d)!",\
                            DF_AU_DSP_NAME( tp_info->a_dev ), iconf->a_channel, tv_ioval );
#endif
                    goto GT_gf_audev_config_clr_rfd;
                }
                tp_info->a_rchannel = iconf->a_channel;
            }
        }
        if( tv_status & DC_AUDEV_SET_FMT ){
            if( tp_info->a_rfmt != iconf->a_fmt ){
                tv_ioval = iconf->a_fmt;
                if( ioctl( tp_info->a_rfd, SNDCTL_DSP_SETFMT, &tv_ioval) < 0 ){
                    // write fmt failed!
                    goto GT_gf_audev_config_clr_rfd;
                }
                if( tv_ioval != (int)(iconf->a_fmt) ){
#if AUDIO_DEVICE_DEBUG
                    DLLOGE("audio.config.%s.record.fmt:err(%d-%d)!",\
                            DF_AU_DSP_NAME( tp_info->a_dev ), iconf->a_fmt, tv_ioval );
#endif
                    goto GT_gf_audev_config_clr_rfd;
                }
                tp_info->a_rfmt = iconf->a_fmt;
            }
        }
        if( tv_status & DC_AUDEV_SET_RATE ){
            if( tp_info->a_rrate != iconf->a_rate ){
                tv_ioval = iconf->a_rate;
                if( ioctl( tp_info->a_rfd, SNDCTL_DSP_SPEED, &tv_ioval ) == -1 ){
                    goto GT_gf_audev_config_clr_rfd;
                }
                if( tv_ioval != (int)(iconf->a_rate) ){
#if AUDIO_DEVICE_DEBUG
                    DLLOGE("audio.config.%s.record.rate:err(%d-%d)!",\
                            DF_AU_DSP_NAME( tp_info->a_dev ), iconf->a_rate, tv_ioval );
#endif
                    goto GT_gf_audev_config_clr_rfd;
                }
                tp_info->a_rrate     = iconf->a_rate;
            }
        }
        tp_info->a_status |= DC_AUDIO_DEVICE_STA_RECORD;
#if AUDIO_DEVICE_DEBUG
    DLLOGI("audio.config.%s.record:success!", DF_AU_DSP_NAME( tp_info->a_dev ) );
#endif
    }else{
#if AUDIO_DEVICE_DEBUG
    DLLOGE("audio.config.%s:error_param!", DF_AU_DSP_NAME( tp_info->a_dev ));
#endif
        return -1;
    }

    return 0;
GT_gf_audev_config_clr_wfd:
#if AUDIO_DEVICE_DEBUG
    DLLOGE("audio.config.%s.speech:err_exit!", DF_AU_DSP_NAME( tp_info->a_dev ) );
#endif
    if( tp_info->a_wfd >= 0  ){
        pthread_mutex_lock( &tp_info->a_wmutex );
        close( tp_info->a_wfd );
        tp_info->a_wfd = -1;
        pthread_mutex_unlock( &tp_info->a_wmutex );
    }
    tp_info->a_status &= ~DC_AUDIO_DEVICE_STA_SPEECK;
    return -1;
GT_gf_audev_config_clr_rfd:
#if AUDIO_DEVICE_DEBUG
    DLLOGE("audio.config.%s.record:err_exit!", DF_AU_DSP_NAME( tp_info->a_dev ));
#endif
     if( tp_info->a_rfd >= 0  ){
        pthread_mutex_lock( &tp_info->a_rmutex );
        close( tp_info->a_rfd );
        tp_info->a_rfd = -1;
        pthread_mutex_unlock( &tp_info->a_rmutex );
    }
    tp_info->a_status &= ~DC_AUDIO_DEVICE_STA_RECORD;
    return -1;
GT_gf_audev_config_err:
#if AUDIO_DEVICE_DEBUG
    DLLOGE("audio.config.%s.others:err_exit!", DF_AU_DSP_NAME( tp_info->a_dev ));
#endif
    return -1;
}



int gf_audev_speeck_getvol( e_au_dev idev ){
    m_au_info       *tp_info = &sv_audev_info[idev];
    int              tv_ret = 0;
    if( (tv_ret = silf_get_speeck_vol( tp_info )) >= 0 )
        tp_info->a_pvol = tv_ret;
    return tv_ret;
}



int gf_audev_speeck_setvol( e_au_dev idev, unsigned ivol ){
    m_au_info       *tp_info = &sv_audev_info[idev];
    int              tv_ret = 0;
    tp_info->a_pvol = ivol;
    tv_ret = silf_set_speeck_vol( tp_info );
    return tv_ret;
}



int gf_audev_speeck_volup( e_au_dev idev, unsigned idelta ){
    m_au_info       *tp_info = &sv_audev_info[idev];
    tp_info->a_pvol = (((int)tp_info->a_pvol + (int)idelta)>100) ? 100 : tp_info->a_pvol+idelta;
    return silf_set_speeck_vol( tp_info );
}



int gf_audev_speeck_voldown( e_au_dev idev, unsigned idelta ){
    m_au_info       *tp_info = &sv_audev_info[idev];
    tp_info->a_pvol = (((int)tp_info->a_pvol - (int)idelta)<0) ? 0 : tp_info->a_pvol+idelta;
    return silf_set_speeck_vol( tp_info );
}



int gf_audev_block_play( e_au_dev idev, char *ibuf, unsigned ilen  ){
    m_au_info   *tp_info = &sv_audev_info[ idev ];
    unsigned     tv_wlen = 0;
    int          tv_ret = 0;

    pthread_mutex_lock( &tp_info->a_wmutex );
    if( tp_info->a_wfd < 0 ){
        // The target devices didnt init or something error happened.
        tv_ret = -1;
        goto GT_gf_audev_block_play_ret;
    }

#if AUDIO_DEVICE_REC_PLAY
    write( sv_recp_fd, ibuf, ilen );
#endif
    while( tv_wlen < ilen ){
        int tv_ret = write( tp_info->a_wfd, &ibuf[ tv_wlen ], ilen - tv_wlen );
        // DLLOGE( "dsp fd -> %d ", tp_info->a_wfd );
        // int tv_ret = write( tv_fd, &ibuf[ tv_wlen ], 1024 );
        // DLLOGV("gf_audev_block_play : write : fd %d ret : %d", tv_fd ,tv_ret );
        if( tv_ret < 0 ){
            close( tp_info->a_wfd );
            tp_info->a_wfd = 0;
            tp_info->a_status = 0;
            tv_ret = -1;
            goto GT_gf_audev_block_play_ret;
         }
        tv_wlen += tv_ret;
    }
    tv_ret = ilen;

GT_gf_audev_block_play_ret:
    pthread_mutex_unlock( &tp_info->a_wmutex );
    return tv_ret;
}



int gf_audev_block_record( e_au_dev idev, void *ibuf, unsigned ilen  ){
    m_au_info   *tp_info = &sv_audev_info[ idev ];
    int          tv_ret = 0;
    unsigned     tv_rlen = 0;
    char        *tp_rbuf = ibuf;

    pthread_mutex_lock( &tp_info->a_rmutex );
    if( tp_info->a_rfd < 0 ){
        // The target devices didnt init or something error happened.
        tv_ret = -1;
        goto GT_gf_audev_block_record_ret;
    }

    while( tv_rlen < ilen ){
        int tv_ret = read( tp_info->a_rfd, &tp_rbuf[ tv_rlen ], ilen - tv_rlen );
#if AUDIO_DEVICE_DEBUG
            // DLLOGD("audev rfd:%d, ret:%d, readlen:%d ", tp_info->a_rfd, tv_ret, ilen);
#endif
        if( tv_ret < 0 ){
            close( tp_info->a_rfd );
            tp_info->a_rfd = 0;
            tp_info->a_status = 0;
            tv_ret = -1;
            goto GT_gf_audev_block_record_ret;
        }else if( tv_ret == 0 ){
            if( tp_info->a_rfd < 0 )
                break;
            usleep(100000);
        }
        tv_rlen += tv_ret;
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
            if( !( tp_info->a_status & DC_AUDIO_DEVICE_STA_INITED ) )
                continue;
            pthread_mutex_lock( &tp_info->a_wmutex );
            if( tp_info->a_wfd >= 0 )
#if AUDIO_DEVICE_DEBUG
                DLLOGV("close dsp ret: %d", close( tp_info->a_wfd ) );
#else
                close( tp_info->a_wfd );
#endif
            tp_info->a_wfd = -1;
            pthread_mutex_unlock( &tp_info->a_wmutex );
            pthread_mutex_lock( &tp_info->a_rmutex );
            if( tp_info->a_rfd >= 0 )
#if AUDIO_DEVICE_DEBUG
                DLLOGV("close dsp ret: %d", close( tp_info->a_rfd ) );
#else
                close( tp_info->a_rfd );
#endif
            tp_info->a_rfd = -1;
            pthread_mutex_unlock( &tp_info->a_rmutex );
            tp_info->a_status |= DC_AUDIO_DEVICE_STA_STASH;
        }
    }else{
        tp_info = &sv_audev_info[ idev ];
        if( !( tp_info->a_status & DC_AUDIO_DEVICE_STA_INITED ) ){
            tv_ret = -1;
            goto GT_gf_audev_release_devices_ret;
        }
        pthread_mutex_lock( &tp_info->a_wmutex );
        if( tp_info->a_wfd >= 0 )
#if AUDIO_DEVICE_DEBUG
            DLLOGV("close dsp ret: %d", close( tp_info->a_wfd ) );
#else
            close( tp_info->a_wfd );
#endif
        tp_info->a_wfd = -1;
        pthread_mutex_unlock( &tp_info->a_wmutex );
        // pthread_mutex_lock( &tp_info->a_rmutex );
        if( tp_info->a_rfd >= 0 )
#if AUDIO_DEVICE_DEBUG
            DLLOGV("close dsp ret: %d", close( tp_info->a_rfd ) );
#else
            close( tp_info->a_rfd );
#endif
        tp_info->a_rfd = -1;
        // pthread_mutex_unlock( &tp_info->a_rmutex );
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
        if( !( tp_info->a_status & DC_AUDIO_DEVICE_STA_INITED ) )
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
            tv_conf.a_fragment  = tp_info->a_wfragment;
            tv_conf.a_pvol      = tp_info->a_pvol;

            tp_info->a_wchannel = 0;
            tp_info->a_wfmt = 0;
            tp_info->a_wrate =0;
            tp_info->a_pvol = -1;

            tp_info->a_status &= ~DC_AUDIO_DEVICE_STA_SPEECK;
            if( gf_audev_config( &tv_conf ) )
                DLLOGE( "device : %s -> init failed!", DF_AU_DSP_NAME( _i ) );
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
                DLLOGE( "device : %s -> init failed!", DF_AU_DSP_NAME( _i ) );
        }
    }
    tv_ret = 0;
GT_gf_audev_recovery_devices_ret:
    return tv_ret;
}

int gf_audev_get_devid_from_name( char *idev_name ){
    if( strcmp( idev_name, DC_AU_DSP_NAME ) == 0 )
        return AUDIO_DEVICE_0;
    else if( strcmp( idev_name, DC_AU_DSP1_NAME ) == 0 )
        return AUDIO_DEVICE_1;
    else if( strcmp( idev_name, DC_AU_DSP2_NAME ) == 0 )
        return AUDIO_DEVICE_2;
    else if( strcmp( idev_name, DC_AU_DSP3_NAME ) == 0 )
        return AUDIO_DEVICE_3;
    return -1;
}




// ---------------------------------------------------------------------------
static inline int silf_get_speeck_vol( m_au_info *iinfo ){
    char        *tp_mixer =     DF_AU_MIXER_NAME( iinfo->a_dev );
    int          tv_fd = 0;
    int          tv_vol = 0;

    if( (tv_fd = open( tp_mixer, O_RDWR )) < 0 ){
        // request mixer handle failed!
        return -1;
    }
    // DLLOGV( "open mixer -> %s %d", tp_mixer, tv_fd );
    if( ioctl( tv_fd, MIXER_READ(SOUND_MIXER_VOLUME), &tv_vol ) == -1 ){
        // get speeck vol failed!
        tv_vol = -1;
    }
    close( tv_fd );
    return tv_vol;
}



static inline int silf_set_speeck_vol( m_au_info *iinfo ){
    char        *tp_mixer =     DF_AU_MIXER_NAME( iinfo->a_dev );
    int          tv_ioval;
    int          tv_fd = 0;
    int          tv_vol = iinfo->a_pvol;

    if( (tv_fd = open( tp_mixer, O_RDWR )) < 0 ){
        // request mixer handle failed!
        return -1;
    }
    // DLLOGV( "open mixer -> %s %d", tp_mixer, tv_fd );
    tv_ioval = SOUND_MIXER_SPEAKER;
    if( ioctl( tv_fd, MIXER_WRITE(SOUND_MIXER_OUTSRC), &tv_ioval ) < 0 ){
        // set speeck mode failed!
        close( tv_fd );
        return -1;
    }
    if( ( tv_vol >= 0) && ( tv_vol <= 100 ) ){
        tv_ioval = tv_vol;
    }else{
        // input vol out of range, set default volume 100.
        tv_ioval = 100;
    }
#if 1
    if( ioctl( tv_fd, SOUND_MIXER_WRITE_VOLUME, &tv_ioval ) == -1) {
        // set speeck vol failed!
        close( tv_fd );
        return -1;
    }
#else
    if( ioctl( tv_fd, MIXER_WRITE(SOUND_MIXER_VOLUME), &tv_ioval ) == -1 ){
        // set speeck vol failed!
        close( tv_fd );
        return -1;
    }
#endif
    close( tv_fd );
    return 0;
}



static inline int silf_set_record_vol( m_au_info *iinfo ){
    char        *tp_mixer =     DF_AU_MIXER_NAME( iinfo->a_dev );
    int          tv_ioval;
    int          tv_fd = 0;
    int          tv_vol = iinfo->a_rvol;

    if( (tv_fd = open( tp_mixer, O_RDWR )) < 0 ){
        // request mixer handle failed!
        return -1;
    }
    // DLLOGV( "open mixer -> %s %d", tp_mixer, tv_fd );

    tv_ioval = SOUND_MIXER_MIC;
    if( ioctl( tv_fd, SOUND_MIXER_WRITE_RECSRC, &tv_ioval ) < 0 ){
        // set speeck mode failed!
        close( tv_fd );
        return -1;
    }

    if( ( tv_vol >= 0) && ( tv_vol <= 100 ) ){
        tv_ioval = tv_vol;
    }else{
        // input vol out of range, set default volume 100.
        tv_ioval = 100;
    }
    if (ioctl( tv_fd, SOUND_MIXER_WRITE_MIC, &tv_ioval ) == -1) {
        // set speeck vol failed!
        close( tv_fd );
        return -1;
    }

    close( tv_fd );
    return 0;
}


#if 1
static inline int silf_set_update_dsp_handle( m_au_info *iinfo, unsigned istatus ){
    char    *tp_dsp = DF_AU_DSP_NAME( iinfo->a_dev );
    int      tv_fd;
    int      tv_flag = 0;
    int      tv_reconn_times = DC_RECONNECT_TIMES;

    if( istatus & DC_AUDEV_SET_SPEEK ){
        tv_flag = O_WRONLY;
    }else if( istatus & DC_AUDEV_SET_RECORD ){
        tv_flag = O_RDONLY;
        if( iinfo->a_dev == AUDIO_DEVICE_1 )
            tv_flag |= O_NONBLOCK;
    }

    while( ( tv_fd = open( tp_dsp, tv_flag ) ) < 0 ){
        // info( "%s retry open", tp_dsp );
        usleep( DC_RECONNECT_INTERVAL );
        if( --tv_reconn_times == 0 ) break;
    }
    if( tv_reconn_times == 0 ) return -1;
    if( fcntl( tv_fd, F_SETFD, FD_CLOEXEC) < 0 ){
        close( tv_fd );
        return -1;
    }

    if( istatus & DC_AUDEV_SET_SPEEK ){
#if AUDIO_DEVICE_DEBUG
        DLLOGV( "open dsp w -> %s %d", tp_dsp, tv_fd );
#endif
        iinfo->a_wfd = tv_fd;
    }else if( istatus & DC_AUDEV_SET_RECORD ){
#if AUDIO_DEVICE_DEBUG
        DLLOGV( "open dsp r -> %s %d", tp_dsp, tv_fd );
#endif
        iinfo->a_rfd = tv_fd;
    }
    return 0;
}
#else
static inline int silf_set_update_dsp_handle( m_au_info *iinfo, unsigned istatus ){

    char    *tp_dsp = DF_AU_DSP_NAME( iinfo->a_dev );
    int      tv_fd;
    int      tv_flag = 0;
    int      tv_reconn_times = DC_RECONNECT_TIMES;

    if( iinfo->a_status & DC_AUDIO_DEVICE_STA_UPDATEFD_FD ){
        iinfo->a_status &= ~DC_AUDIO_DEVICE_STA_UPDATEFD_FD;
        if( iinfo->a_fd >= 0 ){
            close( iinfo->a_fd  );
            iinfo->a_fd = 0;
        }
        if( (istatus & DC_AUDEV_SET_RECORD )\
         && (istatus & DC_AUDEV_SET_SPEEK ) )
            tv_flag = O_RDWR;
        else if( istatus & DC_AUDEV_SET_RECORD )
            tv_flag = O_RDONLY;
        else if( istatus & DC_AUDEV_SET_SPEEK )
            tv_flag = O_WRONLY;

        // DLLOGV( "open dsp -> %s %d", tp_dsp, iinfo->a_fd );

        while( ( tv_fd = open( tp_dsp, tv_flag ) ) < 0 ){
            // info( "%s retry open", tp_dsp );
            usleep( DC_RECONNECT_INTERVAL );
            if( --tv_reconn_times == 0 ) break;
        }
        if( tv_reconn_times == 0 ) return -1;
        if( fcntl( tv_fd, F_SETFD, FD_CLOEXEC) < 0 ){
            close( tv_fd );
            return -1;
        }
        iinfo->a_fd = tv_fd;
        DLLOGV( "open dsp -> %s %d", tp_dsp, iinfo->a_fd );
        // get dsp handle success!
    }
    return 0;
}
#endif




