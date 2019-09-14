#ifndef __AUDIO_DEVICE_H
#define __AUDIO_DEVICE_H

#include <sys/soundcard.h>

typedef enum {
    AUDIO_FMT_S8          =   8,
//    AUDIO_FMT_U8          =   1,
    AUDIO_FMT_S16_LE      =   16,
//    AUDIO_FMT_S16_BE      =   3,
//    AUDIO_FMT_U16_LE      =   4,
//    AUDIO_FMT_U16_BE      =   5,
    AUDIO_FMT_S24_3LE     =   24,
//    AUDIO_FMT_S24_3BE     =   7,
//    AUDIO_FMT_U24_3LE     =   8,
//    AUDIO_FMT_U24_3BE     =   9,
}e_au_fmt;

typedef enum {
    AUDIO_SAMPLE_RATE_8000  = 8000,
    AUDIO_SAMPLE_RATE_11025 = 11025,
    AUDIO_SAMPLE_RATE_12000 = 12000,
    AUDIO_SAMPLE_RATE_16000 = 16000,
    AUDIO_SAMPLE_RATE_22050 = 22050,
    AUDIO_SAMPLE_RATE_24000 = 24000,
    AUDIO_SAMPLE_RATE_32000 = 32000,
    AUDIO_SAMPLE_RATE_44100 = 44100,
    AUDIO_SAMPLE_RATE_48000 = 48000,
    AUDIO_SAMPLE_RATE_96000 = 96000,
    AUDIO_SAMPLE_RATE_19200 = 192000,
}e_au_samplerate;

typedef enum {
    AUDIO_CHANNEL_1 = 1 ,
    AUDIO_CHANNEL_2 = 2 ,
}e_au_channel;

typedef enum {
    AUDIO_DEVICE_0      = 0,   // oss: dsp alsa: c0d0
    AUDIO_DEVICE_1      = 1,   // oss: dsp1 alsa: c0d1
    AUDIO_DEVICE_2      = 2,   // oss: dsp2 alsa: 
    AUDIO_DEVICE_3      = 3,   // oss: dsp3 alsa: c0d2
    AUDIO_DEVICE_ALL   =  4,
}e_au_dev;

#define DC_AU_DSP_NAME      "/dev/dsp"
#define DC_AU_DSP1_NAME     "/dev/dsp1"
#define DC_AU_DSP2_NAME     "/dev/dsp2"
#define DC_AU_DSP3_NAME     "/dev/dsp3"
#define DF_AU_DSP_NAME(_x)  ((_x)==AUDIO_DEVICE_0)?DC_AU_DSP_NAME:\
                                  (((_x)==AUDIO_DEVICE_1)?DC_AU_DSP1_NAME:\
                                   (((_x)==AUDIO_DEVICE_2)?DC_AU_DSP2_NAME:DC_AU_DSP3_NAME))

#define DC_AU_MIX_NAME      "/dev/mixer"
#define DC_AU_MIX1_NAME     "/dev/mixer1"
#define DC_AU_MIX2_NAME     "/dev/mixer2"
#define DC_AU_MIX3_NAME     "/dev/mixer3"
#define DF_AU_MIXER_NAME(_x)  ((_x)==AUDIO_DEVICE_0)?DC_AU_MIX_NAME:\
                                    (((_x)==AUDIO_DEVICE_1)?DC_AU_MIX1_NAME:\
                                     (((_x)==AUDIO_DEVICE_2)?DC_AU_MIX2_NAME:DC_AU_MIX3_NAME))

#define DF_AU_ALSA_DEV(_x)  ((_x)==AUDIO_DEVICE_0)?0:\
                                    (((_x)==AUDIO_DEVICE_1)?1:\
                                     (((_x)==AUDIO_DEVICE_3)?2:AUDIO_DEVICE_ALL))

typedef struct{
#define DC_AUDEV_SET_FORCE      0x80
#define DC_AUDEV_SET_SPEEK      0x40
#define DC_AUDEV_SET_RECORD     0x20
#define DC_AUDEV_SET_VOL        0x01
#define DC_AUDEV_SET_CHANNEL    0x02
#define DC_AUDEV_SET_FMT        0x04
#define DC_AUDEV_SET_RATE       0x08
#define DC_AUDEV_SET_FRAGMENT   0x10
    unsigned        a_status;
    e_au_dev        a_num;
    e_au_channel    a_channel;
    e_au_fmt        a_fmt;
    e_au_samplerate a_rate;
    unsigned        a_pvol;
    unsigned        a_rvol;
    int             a_fragment;
//    int             a_fd;
}m_au_conf;


/*
@ abstract:
    config audio device!

@ param:
    iconf

@ return:
    0       success
    -1      failed

*/
// extern void gf_audev_init( void );
extern void gf_audev_uninit( void );
extern int gf_audev_config( m_au_conf *iconf );
extern int gf_audev_speeck_getvol( e_au_dev idev );
extern int gf_audev_speeck_volup( e_au_dev idev, unsigned idelta );
extern int gf_audev_speeck_voldown( e_au_dev idev, unsigned idelta );
extern int gf_audev_block_play( e_au_dev idev, char *ibuf, unsigned ilen  );
extern int gf_audev_block_record( e_au_dev idev, void *ibuf, unsigned ilen  );
extern int gf_audev_release_devices( e_au_dev idev );
extern int gf_audev_recovery_devices( e_au_dev idev );
extern int gf_audev_speeck_setvol( e_au_dev idev, unsigned ivol );
extern int gf_audev_get_devid_from_name( char *idev_name );



#endif

