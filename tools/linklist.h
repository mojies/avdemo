#ifndef __LINKLIST_H
#define __LINKLIST_H

#include <pthread.h>

/*
index id cant big than 0xFFFF
*/
extern void *gfListCreate( void );
extern int gfListDestory( void *iListHead );
extern int gfListPush( void *iListHead, void *iData );
extern void *gfListGetFirst( void *iListHead );
extern void *gfListPopFirst( void *iListHead );
extern void *gfListPopLast( void *iListHead );
extern void *gfListForEach(void *iListHead, void *iLastData);
extern int gfListPushAssignIndex( void *iListHead, unsigned short iIndex, void *iData );
extern int gfListDelViaIndex( void *iListHead, int iIndex );
extern int gfListDelViaData( void *iListHead, void *iData );
extern int gfListDelList( void *iListHead );
extern int gfListModify(void *iListHead, int iIndex, void *iData );
extern void *gfListSearch(void *iListHead, int iIndex);
#endif
