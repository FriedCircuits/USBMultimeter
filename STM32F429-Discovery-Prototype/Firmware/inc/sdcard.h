#ifndef _SDCARD_H
#define _SDCARD_H

#include "ch.h"

bool sdcardInit(void);
bool sdcardReady(void);
void sdCardEvent(void);
void _sdcardHandlerInsert(eventid_t id);
void _sdcardHandlerRemove(eventid_t id);

#endif /* _SDCARD_H */
