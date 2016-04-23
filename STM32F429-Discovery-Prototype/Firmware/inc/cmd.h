#ifndef _CMD_H
#define _CMD_H

#include "ch.h"

bool cmdCreate(void);
void cmdDestroy(void);
void cmdPrintf(const char* msg);
void cmdManage(void);

#endif /* _CMD_H */
