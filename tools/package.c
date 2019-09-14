/*
 * client 必须保证发送的数据包是对的格式
 * 通信链路返回的数据包也必须确保 decode 的数据流是从 Magic number 开始的
 * 本协议会维护有限个消息订阅者，消息订阅者的标识信息是 connection 的句柄
 * 当 client 发送了消息订阅包，则此 connection 将会被加入订阅队列
 * 当 client 发送了错误的包，或者取消订阅包，此 connection 会从订阅队列中除名
 *
 *
 * */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>

#include "package.h"
#include "Debug.h"

// -----------------------------------------------------------------------------
#define PKG_HEAD_MAGIC_WORD     0xDEADBEEF

typedef struct{
    int pdata;
    void *next;
}mSubber;

// -----------------------------------------------------------------------------
static int sfSubberAdd( int ipdata );
static int sfSubberDel( int ipdata );

// -----------------------------------------------------------------------------
static char const sv_pkg_subscribe[ ]={
    (PKG_HEAD_MAGIC_WORD>>0)&0xFF,
    (PKG_HEAD_MAGIC_WORD>>8)&0xFF,
    (PKG_HEAD_MAGIC_WORD>>16)&0xFF,
    (PKG_HEAD_MAGIC_WORD>>24)&0xFF,
    (G_MSG_TYPE_SUBSCRIBE>>0)&0xFF,
    (G_MSG_TYPE_SUBSCRIBE>>8)&0xFF,
    0x00, 0x00
};
static char const sv_pkg_unsubscribe[ ]={
    (PKG_HEAD_MAGIC_WORD>>0)&0xFF,
    (PKG_HEAD_MAGIC_WORD>>8)&0xFF,
    (PKG_HEAD_MAGIC_WORD>>16)&0xFF,
    (PKG_HEAD_MAGIC_WORD>>24)&0xFF,
    (G_MSG_TYPE_UNSUBSCRIBE>>0)&0xFF,
    (G_MSG_TYPE_UNSUBSCRIBE>>8)&0xFF,
    0x00, 0x00
};
static mSubber *sv_subscriber = NULL;

// -----------------------------------------------------------------------------
/*
 * 1. 确保传入的是一个包
 * 2. 当这个包是一个控制消息的时候，控制消息会提取出来并且返回给 imsg_point
 * 3. 当 imsg_point == NULL 的时候，此消息将会被忽略
 * 4. imsg_point 返回的消息在使用后一定要用 free 释放资源
 * 5. 返回值 >0 消息体长度，-1 此数据不是一个包， -2 不支持消息类型
 * */
//
int msgPkgDecode( void *ibuf, int len, int ipdata,\
        char **imsg_point ){
    uint16_t    tv_msgtype;
    uint16_t    tv_msglen;
    char       *tp_msg;
    int         tv_rlen = 0;

    if( ibuf == NULL ) return -1;
    if( imsg_point ) *imsg_point = NULL;

    tp_msg = ibuf;
    if( !( ( tp_msg[0] == (char)0xef )\
        && ( tp_msg[1] == (char)0xbe )\
        && ( tp_msg[2] == (char)0xad )\
        && ( tp_msg[3] == (char)0xde ) ) )
        return -1;
    tp_msg += 4;
    tv_msgtype = tp_msg[0] + (tp_msg[1]<<8);
    tp_msg += 2;
    tv_msglen = tp_msg[0] + (tp_msg[1]<<8);
    tp_msg += 2;
    tv_rlen = 8 + tv_msglen;
    // DLLOGI( "-> %x - %d", (unsigned)tv_msgtype, (int)tv_msglen );
    // DLLOGI( "-> %s", tp_msg );
    switch( tv_msgtype ){
    case G_MSG_TYPE_SUBSCRIBE:{ // subscriber
        sfSubberAdd( ipdata );
        package_show_subscribers();
        // send subscribe success
    }break;
    case G_MSG_TYPE_UNSUBSCRIBE:{
        sfSubberDel( ipdata );
        // send unsubscribe success
        package_show_subscribers();
    }break;
    case G_MSG_TYPE_DRICTIVE:{ // client to server msg
        char *tp_payload;

        // DLLOGI( "\tG_MSG_TYPE_DRICTIVE" );
        if( imsg_point != NULL ){
            tp_payload = malloc( tv_msglen + 1 );
            memcpy( tp_payload, tp_msg, tv_msglen );
            tp_payload[ tv_msglen ] = '\0';
            *imsg_point = tp_payload;
        }
    }break;
    default:{
        return -2;
    }break;
    }
    return tv_rlen;
}

/*
 * 1. 参数传入必要的信息，此函数将生成一个消息包
 * 2. 生成的包会通过 ipkg 返回，并此函数会返回消息包的长度
 * 3. 消息包使用完之后必须释放
 * */
int msgPkgEncode( void *ibuf, int ilen, uint32_t icmd,\
        const char *imsg, int imsglen ){
    char   *tp_msg;

    if( ibuf ) tp_msg = ibuf;
    else return -1;

    switch( icmd ){
    case G_MSG_TYPE_SUBSCRIBE:{
        if( ilen < 8 ) return -1;
        memcpy( ibuf, sv_pkg_subscribe, sizeof( sv_pkg_subscribe ) );
        return 8 ;
    }break;
    case G_MSG_TYPE_UNSUBSCRIBE:{
        if( ilen < 8 ) return -1;
        memcpy( ibuf, sv_pkg_unsubscribe, sizeof( sv_pkg_unsubscribe ) );
        return 8 ;
    }break;
    case G_MSG_TYPE_DRICTIVE:{
        if( ilen < 8 + imsglen ) return -1;
        tp_msg[ 0 ] = (PKG_HEAD_MAGIC_WORD&0xFF);
        tp_msg[ 1 ] = (PKG_HEAD_MAGIC_WORD>>8)&0xFF;
        tp_msg[ 2 ] = (PKG_HEAD_MAGIC_WORD>>16)&0xFF;
        tp_msg[ 3 ] = (PKG_HEAD_MAGIC_WORD>>24)&0xFF;
        tp_msg[ 4 ] = (G_MSG_TYPE_DRICTIVE>>0)&0xFF;
        tp_msg[ 5 ] = (G_MSG_TYPE_DRICTIVE>>8)&0xFF;
        tp_msg[ 6 ] = (imsglen>>0)&0xFF;
        tp_msg[ 7 ] = (imsglen>>8)&0xFF;
        memcpy( &tp_msg[8], imsg, imsglen );
        return 8 + imsglen;
    }break;
    case G_MSG_TYPE_EVENT:{
        if( ilen < 8 + imsglen ) return -1;
        tp_msg[ 0 ] = (PKG_HEAD_MAGIC_WORD&0xFF);
        tp_msg[ 1 ] = (PKG_HEAD_MAGIC_WORD>>8)&0xFF;
        tp_msg[ 2 ] = (PKG_HEAD_MAGIC_WORD>>16)&0xFF;
        tp_msg[ 3 ] = (PKG_HEAD_MAGIC_WORD>>24)&0xFF;
        tp_msg[ 4 ] = (G_MSG_TYPE_EVENT>>0)&0xFF;
        tp_msg[ 5 ] = (G_MSG_TYPE_EVENT>>8)&0xFF;
        tp_msg[ 6 ] = (imsglen>>0)&0xFF;
        tp_msg[ 7 ] = (imsglen>>8)&0xFF;
        memcpy( &tp_msg[8], imsg, imsglen );
        return 8 + imsglen;
    }break;
    }
    return 0;
}

/*
 * 1. foreach 订阅者
 **/
int msgPkgForeachSubscriber( void *isubber ){
    static mSubber      *tp_node = NULL;

    if( isubber == NULL )
        tp_node = sv_subscriber;
    else if( tp_node )
            tp_node = tp_node->next;
    if( tp_node )
        return tp_node->pdata;
    else
        return (int)(0);
}

int package_show_subscribers( void ){
    mSubber *tp_node;
    DLLOGI( "------------------" );
    tp_node = sv_subscriber;
    while( tp_node ){
        DLLOGI( "\tSubber : %d", tp_node->pdata );
        tp_node = tp_node->next;
    }
    return 0;
}

// -----------------------------------------------------------------------------
static int sfSubberAdd( int ipdata ){
    mSubber            *tp_prev = NULL;
    mSubber            *tp_node = NULL;

    if( ( tp_node = malloc(sizeof(mSubber))) == NULL )
        return -1;
    tp_node->pdata = ipdata;
    tp_node->next = NULL;
    if( sv_subscriber == NULL )
        sv_subscriber = tp_node;
    else{
        tp_prev = sv_subscriber;
        while( tp_prev->pdata != ipdata\
                && tp_prev->next != NULL )
            tp_prev = tp_prev->next;
        if( tp_prev->pdata == ipdata )
            return -1;
        tp_prev->next = tp_node;
    }
    return 0;
}

static int sfSubberDel( int ipdata ){
    mSubber            *tp_prev = NULL;
    mSubber            *tp_node = NULL;

    if( sv_subscriber == NULL )
        return 0;

    tp_node = sv_subscriber;
    while( tp_node->pdata != ipdata ){
        tp_prev = tp_node;
        tp_node = tp_node->next;
    }
    if( tp_node == NULL )
        return 0;
    if( tp_prev )
        tp_prev->next = tp_node->next;
    else
        sv_subscriber = tp_node->next;
    free( tp_node );
    return 0;
}


