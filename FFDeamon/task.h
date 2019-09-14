#ifndef __TASK_H
#define __TASK_H

typedef enum{
    EMsgRoleMusic,
    EMsgRoleTone,
}eMsgRole;

typedef enum{
    EMsgActionTone,
    EMsgActionPlay,
    EMsgActionInsert,
    EMsgActionStop,
    EMsgActionPause,
    EMsgActionResume,
    EMsgActionChangeOutput,
}eMsgAction;

typedef enum{
    EAuOutputUDS,
    EAuOutputOSS,
}eAuOutputType;

typedef enum{
    EAuUrlUDS,
    EAuUrlFile,
    EAuUrlOthers,
}eAuUrlType;

typedef enum{
    EAuFormatMP3,
    EAuFormatPCM,
}eAuFormatType;

typedef struct{
    int             a_musicvol;
    int             a_tonevol;
}mAuVol;

typedef struct{
    eAuUrlType      a_type;
    char           *a_path;
}mUrl;

typedef struct{
    eAuFormatType   a_type;
    int             a_channel;
    int             a_fmt;
    int             a_rate;
}mAuFormat;

typedef struct{
    eMsgAction      a_action;
    mAuVol          a_vol;
    mUrl            a_url;
    union{
    mAuFormat       a_iformat;
    eAuOutputType   a_output;
    };
}mTaskNode;

extern int task_init( int (*icb)(void) );
extern int task_create_ctrlpoint( mTaskNode **inode );
extern int task_destory_ctrlpoint( mTaskNode *inode );
extern int task_list_push( mTaskNode *inode );
extern mTaskNode *task_list_pop( void );

#endif

