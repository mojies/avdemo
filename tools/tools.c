#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include <string.h>
#include <dirent.h>

#include "../tools/Debug.h"

int get_exec_pid( const char *iexename, int *ipidbuf, int ibuflen ){
    DIR                *tv_dir;
    struct dirent      *tv_dptr;
    char               *tp_name;
    int                 tv_num = 0;
    int                 i;

    if( ( tv_dir = opendir( "/proc" ) ) == NULL ){
        DLLOGE( "Cant open /proc" );
        return -1;
    }

    while( (tv_dptr = readdir(tv_dir)) != NULL ){
        int     tv_namelen;
        char    tv_fullpath[128];
        int     tv_pid;
        char    tv_exename[128];
        FILE   *tv_fp;

        if( tv_dptr->d_type != DT_DIR  )
            continue;

        tp_name = tv_dptr->d_name;
        tv_namelen = strlen( tp_name );
        for( i=0; i<tv_namelen; i++ )
            if( ( tv_dptr->d_name[i] < '0' )\
             || ( tv_dptr->d_name[i] > '9' ) )
                break;

         if( i != tv_namelen )
            continue;

        tv_pid = atoi( tp_name );
        tv_fullpath[ sprintf( tv_fullpath, "/proc/%d/comm", tv_pid ) ] = '\0';
        if( access( tv_fullpath, F_OK ) )
            continue;
        if( ( tv_fp = fopen( tv_fullpath, "r" ) ) == NULL )
            continue;
        if( fgets( tv_exename, 128, tv_fp ) == NULL ){
            fclose( tv_fp );
            continue;
        }
        fclose( tv_fp );

        for( i=0; i<(int)(strlen(tv_exename)+1); i++ )
            if( ( tv_exename[i] == '\r' )\
             || ( tv_exename[i] == '\n' ) ){
                tv_exename[i] = '\0';
                break;
            }

        if( strcmp( tv_exename, iexename ) )
            continue;

        ipidbuf[ tv_num ] = tv_pid;
        tv_num++;
        if( tv_num == ibuflen )
            break;
    }
    return tv_num;
}


