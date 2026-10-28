/*=================================================================
	Stash sort button.

=================================================================*/
#pragma once

#include "common.h"

extern bool active_sortStash;

void sortStash(Unit* ptChar);
void sortStashClientItem(DWORD unitId, DWORD page, DWORD pos);
void sortStashClientDone(DWORD nbPages, DWORD isShared, DWORD nbItems);

void Install_SortStash();

/*================================= END OF FILE =================================*/