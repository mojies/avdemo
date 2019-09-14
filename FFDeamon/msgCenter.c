#include "../tools/unixdomain_socket.h"
#include "../tools/cjson.h"
#include "../tools/package.h"
#include "../tools/Debug.h"
#include "task.h"
#include "msgCenter.h"

// -----------------------------------------------------------------------------
static int sfUdsSrvCallback( int ifd, struct sockaddr_in *iaddr );

static uds sv_uds;

int msgCenterInit( void ){
    sv_uds = uds_srv_create( G_MSG_CENTER_NODE_SRV,\
            2, sfUdsSrvCallback, UDS_SERVER_TYPE_MSG );
    return 0;
}

int msgCenterUninit( void ){

    return 0;
}

// -----------------------------------------------------------------------------
static inline int silfGetAction( cJSON *ibody, eMsgAction *iaction ){
    cJSON          *tp_action;

    tp_action = cJSON_GetObjectItem( ibody, "ACTION" );
    if( tp_action == NULL ){
        DLLOGE( "You Action not contain ACTION filed!" );
        return -1;
    }

    DLLOGD( "Action : %s", tp_action->valuestring );
    if( strcmp( tp_action->valuestring, "Tone" ) == 0 )
        *iaction = EMsgActionTone;
    else if( strcmp( tp_action->valuestring, "Play" ) == 0 )
        *iaction = EMsgActionPlay;
    else if( strcmp( tp_action->valuestring, "Insert" ) == 0 )
        *iaction = EMsgActionInsert;
    else if( strcmp( tp_action->valuestring, "Stop" ) == 0 )
        *iaction = EMsgActionStop;
    else if( strcmp( tp_action->valuestring, "Pause" ) == 0 )
        *iaction = EMsgActionPause;
    else if( strcmp( tp_action->valuestring, "Resume" ) == 0 )
        *iaction = EMsgActionResume;
    else if( strcmp( tp_action->valuestring, "ChangeOutput" ) == 0 )
        *iaction = EMsgActionChangeOutput;
    else{
        DLLOGE( "You Action not support!" );
        return -1;
    }

    return 0;
}

static inline int silfGetVol( cJSON *ibody, mAuVol *ivol ){
    cJSON          *tp_vol;

    tp_vol = cJSON_GetObjectItem( ibody, "VOL" );
    tp_vol = tp_vol->child;
    if( tp_vol == NULL || tp_vol->next == NULL ){ // music vol, tone vol
        DLLOGE( "You Action not contain VOL filed!" );
        return -1;
    }
    ivol->a_musicvol = tp_vol->valueint;
    ivol->a_tonevol = tp_vol->next->valueint;
    DLLOGD( "SetVol : music -> %d toneset -> %d", \
            ivol->a_musicvol,  ivol->a_tonevol );
    return 0;
}

static inline int silfGetUrl( cJSON *ibody, mUrl *iurl, mAuFormat *iformat ){
    cJSON      *tp_url;
    cJSON      *tp_iformat;
    cJSON      *tp_entry;

    if( ( tp_url = cJSON_GetObjectItem( ibody, "URL" ) ) == NULL )
        return -1;
    if( strncmp( "file:", tp_url->valuestring, 5 ) == 0 ){
        iurl->a_type = EAuUrlFile;
        iurl->a_path = strdup( &tp_url->valuestring[5] );
    }else if( strncmp( "uds:", tp_url->valuestring, 4 ) == 0 ){
        iurl->a_type = EAuUrlUDS;
        iurl->a_path = strdup( &tp_url->valuestring[4] );
        if( ( tp_iformat = cJSON_GetObjectItem( ibody, "IFORMAT" ) ) == NULL )
            return -1;
        if( ( tp_entry = cJSON_GetObjectItem( tp_iformat, "TYPE" ) ) == NULL )
            return -1;
        if( strncmp( "mp3", tp_entry->valuestring, 3 ) == 0 ){
            iformat->a_type = EAuFormatMP3;
        }else if( strncmp( "pcm", tp_entry->valuestring, 3 ) == 0 ){
            iformat->a_type = EAuFormatPCM;
            if( ( tp_entry = cJSON_GetObjectItem( tp_iformat, "CHANNEL" )) == NULL )
                return -1;
            iformat->a_channel = tp_entry->valueint;
            if( ( tp_entry = cJSON_GetObjectItem( tp_iformat, "FMT" )) == NULL )
                return -1;
            iformat->a_fmt = tp_entry->valueint;
            if( ( tp_entry = cJSON_GetObjectItem( tp_iformat, "RATE" )) == NULL )
                return -1;
            iformat->a_rate = tp_entry->valueint;
        }
    }else{
        iurl->a_type = EAuUrlOthers;
        iurl->a_path = strdup( tp_url->valuestring );
    }
    return 0;
}

static inline int silfGetOutput( cJSON *ibody, eAuOutputType *ioutput ){
    cJSON      *tp_output;

    if( ( tp_output = cJSON_GetObjectItem( ibody, "OUTPUT" ) ) == NULL )
        return -1;
    if( strncmp( "UDS", tp_output->valuestring, 3 ) == 0 ){
        *ioutput = EAuOutputUDS;
    }else if( strncmp( "OSS", tp_output->valuestring, 3 ) == 0 ){
        *ioutput = EAuOutputOSS;
    }
    return 0;
}


static int sfMsgParse( const char *imsg, mTaskNode *inode ){
    cJSON          *tp_body;
    cJSON          *tp_entry;

    if( imsg == NULL || inode == NULL )
        return -1;
    tp_body = cJSON_Parse( imsg );
    if( tp_body == NULL ){
        DLLOGE( "Yoou String Not standard JSON formart!" );
        // DLLOGE( "Connection close : %d", ifd );
        return -1;
    }

    if( silfGetAction( tp_body, &(inode->a_action) ) )
        return -1;

    silfGetVol( tp_body, &(inode->a_vol) );

    switch( inode->a_action ){
    case EMsgActionTone:{
        if( silfGetUrl( tp_body, &(inode->a_url), &(inode->a_iformat) ) )
            return -1;

    }break;
    case EMsgActionPlay:{
        if( silfGetUrl( tp_body, &(inode->a_url), &(inode->a_iformat) ) )
            return -1;

    }break;
    case EMsgActionInsert:{
        if( silfGetUrl( tp_body, &(inode->a_url), &(inode->a_iformat) ) )
            return -1;

    }break;
    case EMsgActionStop:{
    }break;
    case EMsgActionPause:{
    }break;
    case EMsgActionResume:{
    }break;
    case EMsgActionChangeOutput:{
        if( silfGetUrl( tp_body, &(inode->a_url), &(inode->a_iformat) ) )
            return -1;

    }break;
    default:break;
    }

    cJSON_Delete( tp_body );
    return 0;

GT_sfMsgParse_Err:
    cJSON_Delete( tp_body );
    return -1;
}

static int sfUdsSrvCallback( int ifd, struct sockaddr_in *iaddr ){
    char        tv_buf[2048];
    int         tv_ret;
    int         tv_rlen;
    char       *tp_jsonstring;
    int         tv_pkglen;

    tv_rlen = 0;
    do{
        tv_ret = read( ifd, &tv_buf[tv_rlen], 2048 - tv_rlen );
        if( tv_ret < 0 ){
            DLLOGE( "read failed" );
            return UDS_SRVCB_RESULT_CLOSE;
        }

        tv_rlen += tv_ret;
        // DLLOGV( "read %d buflen %d", tv_ret, tv_rlen );
        // ------------------------------------------------------------------
        if(  ( tv_pkglen = msgPkgDecode( tv_buf, tv_rlen,\
                        (int)ifd, &tp_jsonstring ) ) < 0 ){
            DLLOGE( "Pakage format error!" );
            return UDS_SRVCB_RESULT_CLOSE;
        }
        if( tp_jsonstring ){
            mTaskNode     *tp_CtrlNode = NULL;

            DLLOGI( "msg -> %s", tp_jsonstring );
            if( task_create_ctrlpoint( &tp_CtrlNode ) ){
                DLLOGE( "Create ControlPoint Failed" );
                free( tp_jsonstring );
                return UDS_SRVCB_RESULT_CLOSE;
            }
            if( sfMsgParse( tp_jsonstring, tp_CtrlNode ) ){
                DLLOGE( "Msg not follow protocal" );
                task_destory_ctrlpoint( tp_CtrlNode );
                return UDS_SRVCB_RESULT_CLOSE;
            }
            // showMsgEntry( tp_CtrlNode );
            task_list_push( tp_CtrlNode );
            // add ctrl node to ctrl list
        }
        free( tp_jsonstring );
        // ------------------------------------------------------------------

        tv_rlen = tv_rlen - tv_pkglen;
        memcpy( tv_buf, &tv_buf[ tv_pkglen ], tv_pkglen );
    }while( tv_rlen > 0 );

    return UDS_SRVCB_RESULT_SUCCESS;
}



