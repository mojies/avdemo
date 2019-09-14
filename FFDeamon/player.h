#ifndef __PLAYER_H
#define __PLAYER_H

#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavformat/avformat.h>
#include <libavdevice/avdevice.h>
#include <libavcodec/avcodec.h>
#include <libavutil/samplefmt.h>
#include <libavfilter/avfilter.h>
#include <libavutil/channel_layout.h>
#include <libavutil/avstring.h>
#include <libavformat/avformat.h>

#define G_PLAYER_STREAM_TONE    0
#define G_PLAYER_STREAM_MUSIC   1

#define G_PLAYER_CMD_PLAY       0x01
#define G_PLAYER_CMD_STOP       0x01
#define G_PLAYER_CMD_PAUSE      0x01
#define G_PLAYER_CMD_RESUME     0x01


extern int player_init( int inbs_stream, uint64_t iochannel, int iofmt, int iorate );
extern int player_open_output_device( int ien_oss, int ien_alsa, int ien_uds );
extern int player_play( int istream, char const *iname );

#endif
