#include "Debug.h"

unsigned debug_level = 8;

int showCjson( cJSON *ipData ){
    DLLOGW( "----------------------------" );
    DLLOGV( "next -> %p", ipData->next );
    DLLOGV( "prev -> %p", ipData->prev );
    DLLOGV( "child -> %p", ipData->child );

    switch( ipData->type ){
    case cJSON_False:
        DLLOGV( "type -> cJSON_False" );
    break;
    case cJSON_True:
        DLLOGV( "type -> cJSON_True" );
    break;
    case cJSON_NULL:
        DLLOGV( "type -> cJSON_NULL" );
    break;
    case cJSON_Number:
        DLLOGV( "type -> cJSON_Number" );
    break;
    case cJSON_String:
        DLLOGV( "type -> cJSON_String" );
    break;
    case cJSON_Array:
        DLLOGV( "type -> cJSON_Array" );
    break;
    case cJSON_Object:
        DLLOGV( "type -> cJSON_Object" );
    break;
    case cJSON_IsReference:
        DLLOGV( "type -> cJSON_IsReference" );
    break;
    case cJSON_StringIsConst:
        DLLOGV( "type -> cJSON_StringIsConst" );
    break;
    default: break;
    }

    DLLOGV( "valuestring -> %s", ipData->valuestring );
    DLLOGV( "valueint -> %d", ipData->valueint );
    DLLOGV( "valuedouble -> %lf", ipData->valuedouble );
    DLLOGV( "string -> %s", ipData->string );

    return 0;
}

