#include "../tools/unixdomain_socket.h"
#include "../tools/Debug.h"
#include "../tools/package.h"

unsigned debug_level = 7;

// static char sv_test_cmg[] = "{\"ACTION\":\"Play\",\"URL\":\"file:/tmp/test.mp3\",\"VOL\":[30,40] }";
static char sv_test_cmg[] = "{\"ACTION\":\"Tone\",\"URL\":\"file:/tmp/test.mp3\",\"VOL\":[30,40] }";
// static char sv_test_cmg[] = "{\"ACTION\":\"Play\",\"URL\":\"uds:/tmp/audio.uds\",\"VOL\":[30,40] }";

static int sfUdsClientServiceCallback( int ifd, struct sockaddr_in *i_addr );

int main( int argc, char *argv[]  ){
    int             tv_fd;
    char            *tv_len = "Im client I comming..." ;
    char            tv_buf[ 4096 ];
    int             tv_wlen;
    int             tv_ret;

    DLLOGV( "Hello world!" );

    tv_fd = uds_cli_create( "/tmp/avDeamon-cli.sock" ,"/tmp/avDeamon.sock",\
            sfUdsClientServiceCallback, UDS_SERVER_TYPE_MSG );

    DLLOGI( "send subscriber" );
    tv_wlen = msgPkgEncode( tv_buf, 4096, G_MSG_TYPE_SUBSCRIBE, NULL, 0 );
    write( tv_fd, tv_buf, tv_wlen );
    sleep(1);

#if 0
    tv_wlen = msgPkgEncode( tv_buf, 4096, G_MSG_TYPE_UNSUBSCRIBE, NULL, 0 );
    write( tv_fd, tv_buf, tv_wlen );
    sleep(5);
#endif

    DLLOGI( "send play" );
    tv_wlen = msgPkgEncode( tv_buf, 4096, G_MSG_TYPE_DRICTIVE, sv_test_cmg, strlen( sv_test_cmg ) );
    DLLOGI( "tv_wlen -> %d", tv_wlen );
    write( tv_fd, tv_buf, tv_wlen );
    sleep(5);

    while( 1 ){
        sleep(10);
    }

    return 0;
}

static int sfUdsClientServiceCallback( int ifd, struct sockaddr_in *i_addr ){
    char    tv_buf[2048];
    int     tv_rlen;

    if( (tv_rlen = read( ifd, tv_buf, 2048 )) <= 0){
        return UDS_SRVCB_RESULT_CLOSE;
    }
    DLLOGI( "read : %d", ifd );
    return 0;
}

