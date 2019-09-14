#ifndef __GLOBAL_PARAMETERS_H
#define __GLOBAL_PARAMETERS_H

#include <stdbool.h>

#define G_OUTPUT_ENABLE_OSS       true
#define G_OUTPUT_ENABLE_ALSA      false
#define G_OUTPUT_ENABLE_UDS       false

#define G_OUTPUT_CHANNEL          1
// #define G_OUTPUT_FORMAT           "s16"
#define G_OUTPUT_FORMAT           "s16p"
#define G_OUTPUT_RATE             48000

#define G_OUTPUT_OSS_NAME         "/dev/dsp"
#define G_OUTPUT_OSS_PVOL         30 // oss play volume

#define G_HTTP_BUFFER_SIZE      4*1024*1024 // 4M

#endif

