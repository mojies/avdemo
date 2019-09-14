#ifndef __PARAM_H
#define __PARAM_H

#define PFLASE      0
#define PTRUE       1

typedef union {
    int         aint;
    float       afloat;
    char       *astr;
    unsigned    auint;
    unsigned    abool;
}u_udata;

typedef enum {
    E_PARAMS_TYPE_INT,
    E_PARAMS_TYPE_UINT,
    E_PARAMS_TYPE_FLOAT,
    E_PARAMS_TYPE_STR,
    E_PARAMS_TYPE_BOOL,
}e_udatatype;

typedef struct{
    const char     *pattern; // pattern str
    const char     *subpattern;
    e_udatatype     type;
    u_udata         data;
}m_params;


extern int param_get( int argc, char *argv[],\
        m_params *iparam, int num_param );

extern int param_show( m_params *iparam, int inum );

#endif

