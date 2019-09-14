#include <stdlib.h>
#include <string.h>
#include "../tools/linklist.h"
#include "task.h"

#include "../tools/Debug.h"

static void *sv_tasklist = NULL;
static int (*wakeup_cb)(void) = NULL;

int task_init( int (*icb)(void) ){
    wakeup_cb = icb;
    if( sv_tasklist == NULL )
        sv_tasklist = gfListCreate();
    return 0;
}

int task_create_ctrlpoint( mTaskNode **inode ){
    mTaskNode *tp_node;

    if( ( tp_node = malloc( sizeof( mTaskNode ) ) ) == NULL ){
        DLLOGE( "Request Memory failed" );
        return -1;
    }

    memset( tp_node, 0x00, sizeof(mTaskNode) );
    tp_node->a_vol.a_musicvol = -1;
    tp_node->a_vol.a_tonevol = -1;

    *inode = tp_node;
    return 0;
}

int task_destory_ctrlpoint( mTaskNode *inode ){
    if( inode == NULL ) return -1;
    if( inode->a_url.a_path )
        free( inode->a_url.a_path );
    free( inode );
    return 0;
}

int task_list_push( mTaskNode *inode ){
    int tv_ret;
    tv_ret =  gfListPush( sv_tasklist, inode );
    if( wakeup_cb )
        wakeup_cb();
    return tv_ret;
}

mTaskNode *task_list_pop( void ){
    return gfListPopFirst( sv_tasklist );
}

int showMsgaEntry( mTaskNode *ientry  ){
    DLLOGW( "----------------------------" );
    DLLOGV( "a_action : %x", ientry->a_action );
    DLLOGV( "vol->music : %d", ientry->a_vol.a_tonevol );
    DLLOGV( "vol->tone : %d", ientry->a_vol.a_musicvol );
    DLLOGV( "url->type : %d", ientry->a_url.a_type );
    DLLOGV( "url->path : %s", ientry->a_url.a_path );
    DLLOGV( "u->format->type : %d", ientry->a_iformat.a_type );
    DLLOGV( "u->format->channel : %d", ientry->a_iformat.a_channel );
    DLLOGV( "u->format->fmt : %d", ientry->a_iformat.a_fmt );
    DLLOGV( "u->format->rate : %d", ientry->a_iformat.a_rate );
    DLLOGV( "u->output-> : %d", ientry->a_output );
    return 0;
}


