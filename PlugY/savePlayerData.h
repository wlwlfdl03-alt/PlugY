/*=================================================================
	File created by Yohann NICOLAS.

	Save Player Custom Data.

=================================================================*/
#pragma once

#include "common.h"

extern bool active_AutoBackup;
extern int maxBackupPerCharacter;

void backupSaveFiles(char* name, int isHardCoreGame, bool isLOD);
void Install_SavePlayerData();

/*================================= END OF FILE =================================*/