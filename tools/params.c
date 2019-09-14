#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "params.h"
#include "Debug.h"



int param_get( int argc, char *argv[], m_params *iparam, int num_param ){
    int                 i, j;

    if( iparam == NULL )
        return -1;
    for( i = 0; i < argc; i++ ){
        if( strcmp( "-h", argv[ i ] ) == 0 ){
            param_show( iparam, num_param );
            return -1;
        }else if( strcmp( "--help", argv[ i ] ) == 0 ){
            if( access( "./README", F_OK ) )
                DLLOGD("Current dir didnt contain README file,"\
                        "Please check this file under project dirctory.");
            else
                system("cat ./README");
            return -1;
        }
        for( j = 0; j < num_param; j++ ){
            // DLLOGD( "param[j]: %d %s %s", j, iparam[j].pattern, iparam[j].subpattern );
            if( ( iparam[j].pattern )\
             && ( strcmp( argv[i], iparam[j].pattern ) == 0) )
                goto GT_param_get_update;
            else if( ( iparam[j].subpattern )\
                  && ( strcmp( argv[i], iparam[j].subpattern ) == 0 ) )
                goto GT_param_get_update;
            else
                continue;
GT_param_get_update:
            // DLLOGD( "argv[i]:%s", argv[i] );
            switch( iparam[j].type ){
            case E_PARAMS_TYPE_INT:
                i++; if( i >= argc ) return -1;
                sscanf( argv[i], "%d", &(iparam[j].data.aint) );
                // DLLOGD( "float -> %d", iparam[j].data.aint );
            break;
            case E_PARAMS_TYPE_UINT:
                i++; if( i >= argc ) return -1;
                sscanf( argv[i], "%u", &(iparam[j].data.auint) );
                // DLLOGD( "float -> %u", iparam[j].data.auint );
            break;
            case E_PARAMS_TYPE_FLOAT:
                i++; if( i >= argc ) return -1;
                sscanf( argv[i], "%f", &(iparam[j].data.afloat) );
                // DLLOGD( "float -> %f", iparam[j].data.afloat );
            break;
            case E_PARAMS_TYPE_STR:
                i++; if( i >= argc ) return -1;
                iparam[j].data.astr = argv[ i ];
            break;
            case E_PARAMS_TYPE_BOOL:
                iparam[j].data.abool = PTRUE;
            break;
            default:break;
            }
            break;
        }
    }
    return 0;
}

int param_show( m_params *iparam, int inum ){
    int i;
    DLLOGV("================================================================================");
    DLLOGV("%-15s| %-15s | value", "option", "sub option" );
    DLLOGV("--------------------------------------------------------------------------------");
    for( i = 0; i < inum; i++ ){
        switch( iparam[i].type ){
        case E_PARAMS_TYPE_INT:
            DLLOGV( "%-15s| %-15s | %-8s | %d", iparam[i].pattern, iparam[i].subpattern,\
                    "int", iparam[i].data.aint );
        break;
        case E_PARAMS_TYPE_UINT:
            DLLOGV( "%-15s| %-15s | %-8s | %u", iparam[i].pattern, iparam[i].subpattern,\
                    "uint", iparam[i].data.auint );
        break;
        case E_PARAMS_TYPE_FLOAT:
            DLLOGV( "%-15s| %-15s | %-8s | %f", iparam[i].pattern, iparam[i].subpattern,\
                    "float", iparam[i].data.afloat );
        break;
        case E_PARAMS_TYPE_STR:
            DLLOGV( "%-15s| %-15s | %-8s | %s", iparam[i].pattern, iparam[i].subpattern,\
                    "string", iparam[i].data.astr );
        break;
        case E_PARAMS_TYPE_BOOL:
        if( iparam[i].data.abool )
            DLLOGV( "%-15s| %-15s | %-8s | true", iparam[i].pattern,\
                    iparam[i].subpattern, "boolean" );
        else
            DLLOGV( "%-15s| %-15s | %-8s | false", iparam[i].pattern,\
                    iparam[i].subpattern, "boolean" );
        break;
        }
    }
    DLLOGV("================================================================================");
}


