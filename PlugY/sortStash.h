/*=================================================================
	Stash sort button.

=================================================================*/
#pragma once

#include "common.h"

extern bool active_sortStash;

void sortStash(Unit* ptChar);
void sortStashClient(DWORD flags, DWORD nbItems, DWORD checksum);

void Install_SortStash();

/*================================= END OF FILE =================================*/