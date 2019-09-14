/* -----------------------------------------------------------------------------
 * 1. 传输的数据使用 json 格式，所有字符加在一起不能超过 2K Bety
 * 2. 字符串后的 \0 结束符一定要附带过来
 * */
#ifndef __MSG_CENTER_H
#define __MSG_CENTER_H

#define G_MSG_CENTER_NODE_SRV   "/tmp/avDeamon.sock"
#define G_MSG_CENTER_NODE_CLI   "/tmp/avDeamon-cli.sock"

int msgCenterInit( void );
int msgCenterUninit( void );


#endif

