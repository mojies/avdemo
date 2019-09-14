/* -----------------------------------------------------------------------------
 * 说明：
 *   UDS, uds 乃 unix domain socket 的简称
 *
 *   该函数包对 unix domain socket 的使用方法有两种，创建 UDS Server 节点之后会
 *   创建一个线程（一个进程只会创建一个监听的线程）来监听客户端连接的接入。
 *       1. 针对流数据类型的 server （创建时需要指定创建 server 的类型），创建好连
 *       接之后，会为这个连接创建一个线程，用来传输数据流。
 *       2. 小型负载的接收发送（类似于httpd），创建 server 节点之后会创建一个线
 *       程来监听客户端的接入，如果客户端接入，则会将这个客户端的句柄加入 epoll
 *       监控队列，然后在监听线程中处理客户端的指令，请求
 * */
#ifndef __UNIXDOMAIN_SOCKET_H
#define __UNIXDOMAIN_SOCKET_H

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <netinet/in.h>
#include <sys/un.h>
#ifdef LIBUV
#include "<uv.h>"
#endif

#define UDS_SRVCB_RESULT_CLOSE      -1
#define UDS_SRVCB_RESULT_SUCCESS    0
typedef int (*m_uds_srv)(int i_clifd, struct sockaddr_in *i_addr) ;

typedef void* uds;

#define UDS_SERVER_TYPE_STREAM      0x00
#define UDS_SERVER_TYPE_MSG         0x01
extern void *uds_srv_create( const char *iaddr,\
        int i_listener_num, m_uds_srv i_uds_srv, unsigned itype );
extern int uds_srv_distroy( void *iuds );
extern int uds_srv_send( void *iuds, char *ibuf, int ilen );

extern int uds_cli_create( char *icli_name, char *isrv_name,\
        m_uds_srv isrv_cb, unsigned itype );

#endif

