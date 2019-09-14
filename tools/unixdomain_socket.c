#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/epoll.h>
#include <signal.h>
#include "unixdomain_socket.h"
#include "Debug.h"

// ----------------------------------------------------------------------------
#define DC_SRV_EPOLL_MONITOR_SIZE       100
#define DC_SRV_EPOLL_EVSIZE             20
// uds role
#define L_UDS_ROLE_SERVER               0x01
#define L_UDS_ROLE_CLIENT               0x02

// ----------------------------------------------------------------------------
typedef struct{
    unsigned                a_role;     // server or client
    int                     a_fd;
    m_uds_srv               a_srv;
    struct sockaddr_in      a_addr;
    pthread_t               a_thrdid;
}m_cli;

typedef struct{
    unsigned                a_role;     // server or client
    int                     a_fd;
    m_uds_srv               a_srv;
    struct sockaddr_un      a_addr;
    unsigned                a_type;
    int                     a_listener_num;
    void                   *a_next;
    m_cli                  *a_clifd;
}m_uds; // unix domain socket

// ----------------------------------------------------------------------------
static m_cli *sf_add_client2list( m_uds *iuds, int ifd, struct sockaddr_in *iaddr );
static int sf_add_fd_to_epoll_monitor( int i_fd, void *i_pdata  );
static int sf_del_fd_from_epoll_monitor( int i_fd, void *i_pdata  );
static void *sf_socket_monitor( void *pdata );
static void *sf_stream_service_thrd( void *pdata );

// ----------------------------------------------------------------------------
static m_uds       *sp_uds_node = NULL;
static int          sv_epoll_fd = -1;
static pthread_t    sv_thrdid = 0;


// ----------------------------------------------------------------------------
void *uds_srv_create( const char *iname, int ilisten_nbs,\
        m_uds_srv isrv_cb, unsigned itype){

    struct sockaddr_un  tv_addr;
    m_uds              *tp_obj;
    int                 tv_fd;

    if( ( itype != UDS_SERVER_TYPE_STREAM )\
     && ( itype != UDS_SERVER_TYPE_MSG ) ){
        DLLOGE( "Please specify correct type" );
        return NULL;
    }
    if( iname == NULL){
        DLLOGE("You didnt input domain name!");
        return NULL;
    }
    if( strlen(iname) >= sizeof( tv_addr.sun_path ) ){
        DLLOGE("You domain name is too long!");
        return NULL;
    }
    if( isrv_cb == NULL ){
        DLLOGV("You didnt input srv_function!");
        return NULL;
    }
    if( ilisten_nbs == 0 ){
        DLLOGE("listen number is 0, are you sure?!");
        return NULL;
    }

    tv_fd = socket( AF_UNIX, SOCK_STREAM, 0 );
    if( tv_fd < 0 )
        DLLOGE("Request socket failed!");
    tv_addr.sun_family = AF_UNIX;
    strcpy( tv_addr.sun_path, iname );
    unlink( iname );
    if( bind( tv_fd, (struct sockaddr *)&tv_addr, sizeof(tv_addr)) < 0 ){
        DLLOGE("Bind failed!");
        goto GT_create_failed;
    }
    if( listen( tv_fd, ilisten_nbs ) < 0 ){
        DLLOGE("Listen failed!");
        goto GT_create_failed;
    }

    if( ( tp_obj = (m_uds*)malloc( sizeof(m_uds) ) ) == NULL  ){
        DLLOGE( "Request memory failed!" );
        goto GT_create_failed;
    }
    memcpy( &(tp_obj->a_addr), &tv_addr, sizeof(struct sockaddr_un) );
    tp_obj->a_role = L_UDS_ROLE_SERVER;
    tp_obj->a_type = itype;
    tp_obj->a_listener_num = ilisten_nbs;
    tp_obj->a_fd = tv_fd;
    tp_obj->a_srv = isrv_cb;
    tp_obj->a_next = NULL;
    tp_obj->a_clifd = calloc( 1, sizeof(m_cli)*ilisten_nbs );

    sf_add_fd_to_epoll_monitor( tv_fd, tp_obj );

    if( sp_uds_node == NULL ){
        sp_uds_node = tp_obj;
    }else{
        m_uds *sp_node = sp_uds_node;
        while( sp_node->a_next != NULL )
            sp_node = sp_node->a_next;
        sp_node->a_next = tp_obj;
    }

    if( sv_thrdid == 0 )
        pthread_create( &sv_thrdid, NULL, sf_socket_monitor, NULL );

    return tp_obj;
GT_create_failed:
    if( tv_fd > 0 )
        close( tv_fd );
    return NULL;
}


int uds_srv_distroy( void *iuds ){
    m_uds *tp_obj = iuds;
    m_uds *tp_preobj = NULL;
    m_uds *tp_node = sp_uds_node;
    int    j;

    if( sp_uds_node == NULL ){
        DLLOGV("Not srv to destory!");
        return -1;
    }

    while( tp_node != NULL )
        if( tp_obj != tp_node ){
            tp_preobj = tp_node;
            tp_node = tp_node->a_next;
        }else{
            break;
        }

    if( tp_obj == NULL ){
        DLLOGV("didnt find target!");
        return 0;
    }

    if( tp_preobj == NULL ) sp_uds_node = tp_node->a_next;
    else tp_preobj->a_next = tp_node->a_next;

    sf_del_fd_from_epoll_monitor( tp_obj->a_fd, tp_obj );
    close( tp_obj->a_fd );
    if( tp_obj->a_clifd ){
        for( j=0; j<tp_obj->a_listener_num; j++ )
            if( tp_obj->a_clifd[j].a_fd > 0 ){
                if( tp_obj->a_type == UDS_SERVER_TYPE_MSG )
                    sf_del_fd_from_epoll_monitor( tp_obj->a_clifd[j].a_fd,\
                            &(tp_obj->a_clifd[j]) );
                close( tp_obj->a_clifd[j].a_fd );
                if( tp_obj->a_clifd[j].a_thrdid ){
                    pthread_kill( tp_obj->a_clifd[j].a_thrdid, SIGHUP );
                    tp_obj->a_clifd[j].a_thrdid = 0;
                }
            }
        free( tp_obj->a_clifd );
    }
    free( tp_obj );

    return 0;
}

int uds_srv_send( void *iuds,  char *ibuf, int ilen ){
    m_uds      *tp_obj = iuds;
    int         i;

    if( tp_obj->a_clifd == NULL)
        return 0;
    for( i=0; i< tp_obj->a_listener_num; i++ ){
        if( tp_obj->a_clifd[i].a_fd <= 0 )
            continue;
        if( write( tp_obj->a_clifd[i].a_fd, ibuf, ilen ) <= 0 ){
            close( tp_obj->a_clifd[i].a_fd );
            sf_del_fd_from_epoll_monitor( tp_obj->a_clifd[i].a_fd,\
                    &(tp_obj->a_clifd[i]) );
            tp_obj->a_clifd[ i ].a_fd = -1;
        }
    }
    return 0;
}

int uds_cli_create( char *icli_name, char *isrv_name, m_uds_srv isrv_cb,\
        unsigned itype ){
    m_uds              *tp_obj;
    struct sockaddr_un  tv_cliun;
    struct sockaddr_un  tv_srvun;
    int                 tv_clifd = -1;
    int                 tv_addrlen;

    if( ( icli_name == NULL )\
            || ( isrv_name == NULL ) ){
        return -1;
    }
    if( (tv_clifd = socket( AF_UNIX, SOCK_STREAM, 0 )) < 0 ){
        DLLOGV( "UDS.reques.socket:failed!" );
        return -1;
    }
    memset( &tv_cliun, 0x00, sizeof(tv_cliun) );
    tv_cliun.sun_family = AF_UNIX;
    strcpy( tv_cliun.sun_path, icli_name );
    tv_addrlen = offsetof( struct sockaddr_un, sun_path ) + strlen( icli_name );
    unlink( tv_cliun.sun_path );

    if( bind( tv_clifd, (struct sockaddr *)&tv_cliun, tv_addrlen ) < 0 ){
        DLLOGV("UDS.client.bind:failed!");
        goto GT_uds_cli_create_err;
    }

    if( chmod( tv_cliun.sun_path, S_IRWXU ) ){
        DLLOGV( "UDS.Client.change_mode:failed!" );
        goto GT_uds_cli_create_err;
    }

    memset( &tv_srvun, 0x00, sizeof( tv_srvun ) );
    tv_srvun.sun_family = AF_UNIX;
    strcpy( tv_srvun.sun_path, isrv_name );
    tv_addrlen = offsetof( struct sockaddr_un, sun_path ) + strlen( isrv_name );
    if( connect( tv_clifd, (struct sockaddr*)&tv_srvun, tv_addrlen ) ){
        DLLOGV( "UDS.Client.connect:failed!" );
        goto GT_uds_cli_create_err;
    }

    if( ( tp_obj = (m_uds*)malloc( sizeof(m_uds) ) ) == NULL  ){
        DLLOGE( "Request memory failed!" );
        goto GT_uds_cli_create_err;
    }
    memset( &(tp_obj->a_addr), 0x00, sizeof(struct sockaddr_un) );
    tp_obj->a_role = L_UDS_ROLE_CLIENT;
    tp_obj->a_type = itype;
    tp_obj->a_fd = tv_clifd;
    
    tp_obj->a_next = NULL;
    tp_obj->a_clifd = NULL;

    sf_add_fd_to_epoll_monitor( tv_clifd, tp_obj );

    if( sp_uds_node == NULL ){
        sp_uds_node = tp_obj;
    }else{
        m_uds *sp_node = sp_uds_node;
        while( sp_node->a_next != NULL )
            sp_node = sp_node->a_next;
        sp_node->a_next = tp_obj;
    }

    if( sv_thrdid == 0 )
        pthread_create( &sv_thrdid, NULL, sf_socket_monitor, NULL );

    return tv_clifd;
GT_uds_cli_create_err:
    if( tv_clifd > 0 ){
        close( tv_clifd );
    }
    return -1;
}

// -------------------------------------------------------------s耳机产品。它不仅能够提供卓越的Sennheiser品质
static void *sf_socket_monitor( void *pdata ){
    struct epoll_event  tv_events[ DC_SRV_EPOLL_EVSIZE ];
    uint32_t            tv_event;
    m_uds              *tp_obj;
    unsigned            tv_role;
    int                 t_ret;
    int                 i,j;

    pthread_detach( pthread_self() );
    while( sp_uds_node ){
        t_ret = epoll_wait( sv_epoll_fd, tv_events, DC_SRV_EPOLL_EVSIZE, 20*1000);
        // DLLOGV("uds epoll_wait ->%d", t_ret );
        if( t_ret < 0 )
            DLLOGE( "uds.listener.epoll:failed : %d", t_ret );
        else if( t_ret == 0 )
            continue;

        for( i=0; i<t_ret; i++ ){
            tv_role = *((unsigned*)tv_events[i].data.ptr);
            tp_obj = (m_uds*)tv_events[i].data.ptr;
            tv_event = tv_events[i].events;
            switch( tv_role ){
            case L_UDS_ROLE_SERVER:{
                struct sockaddr_in  tv_cli_addr;
                socklen_t           tv_addrlen = sizeof(struct sockaddr_in);
                int                 tv_srvfd = tp_obj->a_fd;
                int                 tv_clifd;
                m_cli              *tp_cli;

                tv_clifd = accept( tp_obj->a_fd,\
                        (struct sockaddr*)&tv_cli_addr,&tv_addrlen );

                if( tp_obj->a_type == UDS_SERVER_TYPE_STREAM ){
                    if( ( tp_cli = sf_add_client2list( tp_obj, tv_clifd,\
                                    &tv_cli_addr ) ) == NULL )
                        goto GT_next_epool_event;
                    if( tp_cli->a_thrdid == 0 )
                        pthread_create( &(tp_cli->a_thrdid), NULL,\
                                sf_stream_service_thrd, tp_cli );
                    else
                        DLLOGV( "The last thrd not exit!" );
                }else if( tp_obj->a_type == UDS_SERVER_TYPE_MSG ){
                    int tv_ret;
                    tv_ret = tp_obj->a_srv( tv_clifd, &tv_cli_addr );
                    if( tv_ret == UDS_SRVCB_RESULT_CLOSE )
                        goto GT_next_epool_event;
                    if( ( tp_cli = sf_add_client2list( tp_obj, tv_clifd,\
                                    &tv_cli_addr ) ) == NULL )
                        goto GT_next_epool_event;
                    sf_add_fd_to_epoll_monitor( tp_cli->a_fd, tp_cli );
                }
            }break;
            case L_UDS_ROLE_CLIENT:{
                m_cli              *tp_cli;
                int                 tv_ret;

                tp_cli = (m_cli*)tv_events[i].data.ptr;
                if( ( tv_event&EPOLLHUP )||( tv_event&EPOLLERR ) ){
                    close( tp_cli->a_fd );
                    sf_del_fd_from_epoll_monitor( tp_cli->a_fd, tp_cli );
                    tp_cli->a_fd = -1;
                    break;
                }
                tv_ret = tp_cli->a_srv( tp_cli->a_fd, &tp_cli->a_addr );
                if( tv_ret == UDS_SRVCB_RESULT_CLOSE ){
                    close( tp_cli->a_fd );
                    sf_del_fd_from_epoll_monitor( tp_cli->a_fd, tp_cli );
                    tp_cli->a_fd = -1;
                }
            }break;
            default:break;
            }
GT_next_epool_event:
            ;
        }
#if 0
        {
            m_uds *tp_node = sp_uds_node;
            m_uds *tp_prev = NULL;
            while( tp_node ){
                if( tp_node->a_fd < 0 ){
                    if( tp_prev )
                        tp_prev->a_next = tp_node->a_next;
                    else
                        sp_uds_node = tp_node->a_next;

                    if( tp_node->a_clifd )
                        free( tp_node->a_clifd );
                    free( tp_node );
                    if( tp_prev )
                        tp_node = tp_prev->a_next;
                    else
                        tp_node = sp_uds_node;
                    continue;
                }
                tp_prev = tp_node;
                tp_node = tp_node->a_next;
            }
        }
#endif
    }
    sv_thrdid = 0;
    sv_epoll_fd = -1;
    pthread_exit( NULL );
}

static m_cli *sf_add_client2list( m_uds *iuds, int ifd, struct sockaddr_in *iaddr ){
    int j;

    for( j=0; j < iuds->a_listener_num; j++ )
        if( iuds->a_clifd[j].a_fd <= 0 ){
            iuds->a_clifd[j].a_role = L_UDS_ROLE_CLIENT;
            iuds->a_clifd[j].a_fd = ifd;
            iuds->a_clifd[j].a_srv = iuds->a_srv;
            iuds->a_clifd[j].a_addr = *iaddr;
            DLLOGV("clifd : %d storage : %d", ifd, j );
            break;
        }
    if( j == iuds->a_listener_num ){
        close( iuds->a_clifd[j].a_fd );
        DLLOGV( "no space to listen!" );
        return NULL;
    }

    return &iuds->a_clifd[j];
}

static void sf_stream_thrd_exit( int sig ){
    DLLOGV( "uds -> out %lu", (unsigned long)pthread_self() );
    pthread_exit(NULL);
}

static void *sf_stream_service_thrd( void *pdata ){
    m_cli  *tp_cli;
    tp_cli = pdata;
    pthread_detach( pthread_self() );
    signal( SIGHUP, sf_stream_thrd_exit );
    DLLOGV( "uds -> in %lu", (unsigned long)pthread_self() );
    if( tp_cli->a_srv )
        tp_cli->a_srv( tp_cli->a_fd, &tp_cli->a_addr );
    close( tp_cli->a_fd );
    tp_cli->a_fd = -1;
    tp_cli->a_thrdid = 0;
    DLLOGV( "uds -> out %lu", (unsigned long)pthread_self() );

    pthread_exit( NULL );
}

static int sf_add_fd_to_epoll_monitor( int i_fd, void *i_pdata  ){
    struct epoll_event tv_epdata;

    if( sv_epoll_fd == -1 ){
        sv_epoll_fd = epoll_create( DC_SRV_EPOLL_MONITOR_SIZE );
        if( sv_epoll_fd < 0 )
            DLLOGE("epoll request failed!");
    }
    tv_epdata.events = EPOLLIN|EPOLLHUP|EPOLLERR;
    tv_epdata.data.ptr = i_pdata;
    epoll_ctl( sv_epoll_fd, EPOLL_CTL_ADD, i_fd, &tv_epdata );
}

static int sf_del_fd_from_epoll_monitor( int i_fd, void *i_pdata  ){
    struct epoll_event tv_epdata;

    if( sv_epoll_fd == -1 ){
        DLLOGV("This fd has already removed!");
        return 0;
    }

    tv_epdata.events = EPOLLIN|EPOLLHUP|EPOLLERR;
    tv_epdata.data.ptr = i_pdata;
    epoll_ctl( sv_epoll_fd, EPOLL_CTL_DEL, i_fd, &tv_epdata );
}

