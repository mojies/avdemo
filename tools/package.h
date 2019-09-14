#ifndef __MSG_PKG_H
#define __MSG_PKG_H

#define G_MSG_TYPE_SUBSCRIBE       0x0001
#define G_MSG_TYPE_SUBSCRIBERSP    0x0011
#define G_MSG_TYPE_UNSUBSCRIBE     0x0002
#define G_MSG_TYPE_UNSUBSCRIBERSP  0x0012
#define G_MSG_TYPE_DRICTIVE        0x0003
#define G_MSG_TYPE_DRICTIVERSP     0x0013
#define G_MSG_TYPE_EVENT           0x0004

extern int msgPkgDecode( void *ibuf,int ilen, int ipdata,\
        char **imsg_point );

extern int msgPkgEncode( void *ibuf, int ilen, uint32_t cmd,\
        const char *imsg, int imsglen );

extern int msgPkgForeachSubscriber( void *isubber );
extern int package_show_subscribers( void );

#endif
