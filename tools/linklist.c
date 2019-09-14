#include <stdlib.h>
#include "linklist.h"
#include "Debug.h"

//------------------------------------------------------------------------------
#define DF_LINKLIST_INIT_LISTHEAD \
     {\
     .aNext = NULL,\
     .aIndexCount = 0,\
     .aMutex = PTHREAD_MUTEX_INITIALIZER,\
     }

//------------------------------------------------------------------------------
typedef struct{
    void *aNext;
    unsigned short aIndexCount;
    pthread_mutex_t aMutex;
}mListHead;

typedef struct{
    int      aIndex;
    void    *aData;
    void    *aNext;
}mTarNode;


//------------------------------------------------------------------------------
// get linklist obj
void *gfListCreate( void ){
    mListHead *tListHead;
    if( ( tListHead = (mListHead*)calloc(1, sizeof(mListHead)) ) == NULL )
        return NULL;
    tListHead->aNext = NULL;
    tListHead->aIndexCount = 0;
    pthread_mutex_init( &(tListHead->aMutex), NULL );
    return tListHead;
}

int gfListDestory( void *iListHead ){
    if( gfListDelList( iListHead ) < 0 )
        return -1;
    free( iListHead );
    return 0;
}

// push to list
int gfListPush( void *iListHead, void *iData ){
    mListHead  *tListHead = iListHead;
    int         tIndex;
    mTarNode   *tLastNode;
    mTarNode   *tNode;

    if( iListHead == NULL ) return -1;
    tNode = (mTarNode*)calloc(1, sizeof(mTarNode));
    if( tNode == NULL ) return -1;
    pthread_mutex_lock( &(tListHead->aMutex) );
    tListHead->aIndexCount++;
    tIndex = tListHead->aIndexCount;
    tNode->aIndex = tIndex;
    tNode->aData = iData;
    tLastNode = tListHead->aNext;
    if( tLastNode == NULL ){
        tListHead->aNext = tNode;
    }else{
        while( tLastNode->aNext != NULL )
            tLastNode = tLastNode->aNext;
        tLastNode->aNext = tNode;
    }
    pthread_mutex_unlock( &(tListHead->aMutex) );
    return tIndex;
}

// Get first in list
void *gfListGetFirst( void *iListHead ){
    mListHead  *tListHead = iListHead;
    mTarNode   *tNode;
    void       *ret;

    if( iListHead == NULL ) return NULL;
    if( tListHead->aNext == NULL ) return NULL;
    pthread_mutex_lock( &(tListHead->aMutex) );
    tNode = tListHead->aNext;
    ret = tNode->aData;
    pthread_mutex_unlock( &(tListHead->aMutex) );
    return ret;
}

// pop from first
void *gfListPopFirst( void *iListHead ){
    mListHead  *tListHead = iListHead;
    void       *ret;
    mTarNode   *tNode;

    if( iListHead == NULL ) return NULL;
    if( tListHead->aNext == NULL ) return NULL;
    pthread_mutex_lock( &(tListHead->aMutex) );
    tNode = tListHead->aNext;
    ret = tNode->aData;
    tListHead->aNext = tNode->aNext;
    free( tNode );
    pthread_mutex_unlock( &(tListHead->aMutex) );
    return ret;
}

// pop from last
void *gfListPopLast( void *iListHead ){
    mListHead  *tListHead = iListHead;
    int         tIndex;
    mTarNode   *tPreNode = NULL;
    mTarNode   *tNode;
    void       *ret;

    if( iListHead == NULL ) return NULL;
    if( tListHead->aNext == NULL ) return NULL;
    pthread_mutex_lock( &(tListHead->aMutex) );
    tNode = tListHead->aNext;
    while( tNode->aNext != NULL ){
        tPreNode = tNode;
        tNode = tNode->aNext;
    }
    if( tPreNode )
        tPreNode = NULL;
    else
        tListHead->aNext = NULL;
    ret = tNode->aData;
    free( tNode );
    pthread_mutex_unlock( &(tListHead->aMutex) );
    return ret;
}

// for each
void *gfListForEach(void *iListHead, void *iLastData){
    mListHead  *tListHead = iListHead;
    mTarNode   *tTarNode;
    void       *ret = NULL;

    if( iListHead == NULL ) return NULL;
    if( ((mListHead*)iListHead)->aNext == NULL ) return NULL;
    pthread_mutex_lock( &(tListHead->aMutex) );
    tTarNode = tListHead->aNext;
    if( iLastData != NULL ){
        while( tTarNode->aData != iLastData )
            tTarNode = tTarNode->aNext;
        tTarNode = tTarNode->aNext;
    }
    ret = tTarNode->aData;
    pthread_mutex_unlock( &(tListHead->aMutex) );
    return ret;
}

// 
int gfListPushAssignIndex( void *iListHead, unsigned short iIndex, void *iData ){
    mListHead  *tListHead = iListHead;
    mTarNode   *tNode;
    mTarNode   *tLastNode;

    pthread_mutex_lock( &(tListHead->aMutex) );
    tNode = (mTarNode*)calloc(1, sizeof(mTarNode));
    if( tNode == NULL ) return -1;;
    tNode->aIndex = iIndex;
    tNode->aData = iData;
    tLastNode = tListHead->aNext;
    if( tLastNode == NULL ){
        tListHead->aNext = tNode;
    }else{
        while( tLastNode->aNext != NULL )
            tLastNode = tLastNode->aNext;
        tLastNode = tNode;
    }
    pthread_mutex_unlock( &(tListHead->aMutex) );
    return iIndex;
}

// del
int gfListDelViaIndex( void *iListHead, int iIndex ){
    mListHead  *tListHead = iListHead;
    mTarNode   *tTarNode, *tPreNode;
    int         ret = -1;

    pthread_mutex_lock( &(tListHead->aMutex) );
    if( (iIndex<0)||(iIndex>65535) )
        goto GT_gfListDel_end;
    tTarNode = tListHead->aNext;
    tPreNode = NULL;
    while( (tTarNode != NULL)&&(tTarNode->aIndex != iIndex) ){
        tPreNode = tTarNode;
        tTarNode = tTarNode->aNext;
    }
    if( tTarNode == NULL )
        goto GT_gfListDel_end;
    if( tPreNode == NULL )
        tListHead->aNext = tTarNode->aNext;
    else
        tPreNode->aNext = tTarNode->aNext;
    free(tTarNode);
    ret = 0;
GT_gfListDel_end:
    pthread_mutex_unlock( &(tListHead->aMutex) );
    return ret;
}

int gfListDelViaData( void *iListHead, void *iData ){
    mListHead  *tListHead = iListHead;
    mTarNode   *tTarNode, *tPreNode;
    int         ret = -1;

    pthread_mutex_lock( &(tListHead->aMutex) );
    tTarNode = tListHead->aNext;
    tPreNode = NULL;
    while( (tTarNode != NULL)&&(tTarNode->aData != iData) ){
        tPreNode = tTarNode;
        tTarNode = tTarNode->aNext;
    }
    if( tTarNode == NULL )
        goto GT_gfListDel_end;
    if( tPreNode == NULL )
        tListHead->aNext = tTarNode->aNext;
    else
        tPreNode->aNext = tTarNode->aNext;
    free(tTarNode);
    ret = 0;
GT_gfListDel_end:
    pthread_mutex_unlock( &(tListHead->aMutex) );
    return ret;
}
// delALL
int gfListDelList( void *iListHead ){
    mListHead  *tListHead = iListHead;
    mTarNode   *tTarNode, *tNextNode;

    if( iListHead == NULL ) return -1;
    if( tListHead->aNext == NULL ) return 0;
    pthread_mutex_lock( &(tListHead->aMutex) );
    tTarNode = tListHead->aNext;
    do{
        tNextNode = tTarNode->aNext;
        free( tTarNode );
        tTarNode = tNextNode;
    }while( tTarNode != NULL );
    tListHead->aNext = NULL;
    pthread_mutex_unlock( &(tListHead->aMutex) );
    return 0;
}

// modify
int gfListModify(void *iListHead, int iIndex, void *iData ){
    mListHead  *tListHead = iListHead;
    mTarNode   *tTarNode;
    int         ret = -1;

    pthread_mutex_lock( &(tListHead->aMutex) );
    if( (iIndex<0)||(iIndex>65535) )
        goto GT_gfListModify_end;
    if( tListHead->aNext == NULL )
        goto GT_gfListModify_end;
    tTarNode = tListHead->aNext;
    while( (tTarNode != NULL)&&(tTarNode->aIndex != iIndex) )
        tTarNode = tTarNode->aNext;
    if( tTarNode == NULL )
        goto GT_gfListModify_end;
    tTarNode->aData = iData;
    ret = 0;
    // LLOGW("Modify success!");
GT_gfListModify_end:
    pthread_mutex_unlock( &(tListHead->aMutex) );
    return ret;
}

// search
void *gfListSearch(void *iListHead, int iIndex){
    mListHead  *tListHead = iListHead;
    mTarNode   *tTarNode;
    void       *ret = NULL;

    pthread_mutex_lock( &(tListHead->aMutex) );
    if( (iIndex<0)||(iIndex>65535) )
        goto GT_gfListSearch_end;
    if( tListHead->aNext == NULL )
        goto GT_gfListSearch_end;
    tTarNode = tListHead->aNext;
    while( (tTarNode != NULL)&&(tTarNode->aIndex != iIndex) )
        tTarNode = tTarNode->aNext;
    if( tTarNode == NULL )
        goto GT_gfListSearch_end;
    ret = tTarNode->aData;
GT_gfListSearch_end:
    pthread_mutex_unlock( &(tListHead->aMutex) );
    return ret;
}


