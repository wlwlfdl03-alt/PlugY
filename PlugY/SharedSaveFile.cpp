/*=================================================================
	File created by Yohann NICOLAS.

	Add an extra save file shared by all own characters.

=================================================================*/

#include "sharedSaveFile.h"
#include "infinityStash.h"
#include "customLibraries.h"
#include "common.h"
#include <stdio.h>

#define FILE_SHAREDSTASH 0x00535353	//"SSS "
#define BUFFER_SIZE 0x4000
#define FILE_VERSION 0x3230			//"02"


#define D2S_STATUS_HARDCORE 0x04
#define D2S_STATUS_EXPANSION 0x20

// Return the status byte of the character's .d2s file, or -1 if it can't be read.
int readCharacterStatus(char* name)
{
	char filename[512];
	D2FogGetSavePath( filename, 512-strlen(name)-5);
	strcat(filename,name);
	strcat(filename,".d2s");
	FILE* file = fopen(filename, "rb");
	if (!file)
		return -1;
	BYTE status;
	fseek(file, 0x24, SEEK_SET);
	DWORD nbRead = fread(&status, 1, 1, file);
	fclose(file);
	return nbRead == 1 ? status : -1;
}

bool isExpansionCharacter(char* name)
{
	int status = readCharacterStatus(name);
	log_msg("%s is an expansion character = %d (status=%d)\n", name, status < 0 || (status & D2S_STATUS_EXPANSION), status);
	return status < 0 || (status & D2S_STATUS_EXPANSION);// Unknown : keep LoD behavior.
}

// LoD game : Game+70 (isLODGame), Game+50 in 1.09b (D2Game reads the difficulty at +4D and this flag at +50).
bool isLODGame(Game* ptGame)
{
	return (version_D2Game == V109b ? *(DWORD*)((BYTE*)ptGame + 0x50) : ptGame->isLODGame) != 0;
}

// Diablo II Classic characters use their own shared stash : LoD items can't exist in a Classic game.
const char* getSharedSaveFilePrefix(bool isHardcore, bool isLOD)
{
	if (isLOD)
		return separateHardSoftStash && isHardcore ? "_LOD_HC_" : "_LOD_";
	return separateHardSoftStash && isHardcore ? "_CLASSIC_HC_" : "_CLASSIC_";
}

BYTE* readSharedSaveFile(char* name, DWORD* size, bool isLOD)
{
	char filename[512];
	BYTE* data=NULL;
	BYTE isHardCore=0;
	FILE* file=NULL;

	if (separateHardSoftStash)//Get hardcore flag
	{
		int status = readCharacterStatus(name);
		isHardCore = status >= 0 && (status & D2S_STATUS_HARDCORE);
		log_msg("%s is a HardCore character = %d\n",name,isHardCore);
	}

	if (active_sharedStash)
	{
		D2FogGetSavePath( filename, 512-strlen("_CLASSIC_HC_")-strlen(sharedStashFilename)-5);
		strcat(filename, getSharedSaveFilePrefix(isHardCore != 0, isLOD));
		strcat(filename, sharedStashFilename);
		strcat(filename,".sss");

		log_msg("Shared file to read : %s\n",filename);

		file = fopen(filename, "rb");
	}

	if (file)
	{
		fseek(file, 0, SEEK_END);
		*size = ftell(file);
		fseek(file, 0, SEEK_SET);
		data = (BYTE*)D2FogMemAlloc(*size,__FILE__,__LINE__,0);
		DWORD nbRead = fread(data, 1, *size, file);
		fclose(file);
		d2_assert(nbRead != *size , "nbRead from shared save file != size", __FILE__, __LINE__);
	} else {
		log_msg("Can't open shared save file in mode \"rb\" (is not an error if it's the first start of the mod)\n");
		*size = 14;
		DWORD maxSize = 100;
		data = (BYTE*)D2FogMemAlloc(maxSize,__FILE__,__LINE__,0);
		*((DWORD *)&data[0]) = FILE_SHAREDSTASH;
		*((WORD *)&data[4]) = FILE_VERSION;
		*((DWORD *)&data[6]) = 0;
		*((DWORD *)&data[10]) = 0;// number of stash

		TCustomDll* currentDll = customDlls;
		while (currentDll)
		{
			currentDll->initSharedSaveFile(&data, &maxSize, size);
			currentDll=currentDll->nextDll;
		}
	}

	return data;
}


int loadSharedSaveFile(Unit* ptChar, BYTE data[], DWORD maxSize)
{
	if ( !ptChar || !data) return false;

	log_msg("Load shared file\n");

	DWORD curSize = 0;

	if (*(DWORD*)&data[curSize] != FILE_SHAREDSTASH)
	{
		log_msg("loadSharedSaveFile -> bad header\n");
		return 9;
	}
	curSize += 4;

	if (*(WORD *)&data[curSize] == 0x3130) {//"01"
		curSize += 2;
		PCPY->sharedGold = 0;
	} else if (*(WORD *)&data[curSize] == FILE_VERSION) {
		curSize += 2;
		PCPY->sharedGold = *(DWORD*)&data[curSize];
		curSize += 4;
	} else {
		log_msg("loadSharedSaveFile -> bad file version : %04X\n", *(WORD *)&data[curSize]);
		return 9;
	}

	int ret = loadStashList(ptChar, data, maxSize, &curSize, true);

	TCustomDll* currentDll = customDlls;
	while (!ret && currentDll)
	{
		ret = currentDll->loadSharedSaveFile(ptChar, data, maxSize, &curSize);
		currentDll=currentDll->nextDll;
	}

	PCPY->sharedStashIsOpened = true;
	return ret;
}


void writeSharedSaveFile(char* name, BYTE* data, DWORD size, bool isHardcore, bool isLOD)
{
	char szTempName[MAX_PATH];
	char szSaveName[MAX_PATH];

	//Get temporary savefile name.
	D2FogGetSavePath( szTempName, MAX_PATH-30);
	strcat(szTempName, getSharedSaveFilePrefix(isHardcore, isLOD));
	strcat(szTempName, sharedStashFilename);
	strcat(szTempName,".ss~");
	log_msg("Shared temporary file for saving : %s\n", szTempName);

	//Write data in savefile.
	FILE* customSaveFile = fopen(szTempName, "wb+");
	fwrite(data, size, 1, customSaveFile);
	fclose(customSaveFile);

	//Get real savefile name.
	D2FogGetSavePath( szSaveName, MAX_PATH-30);
	strcat(szSaveName, getSharedSaveFilePrefix(isHardcore, isLOD));
	strcat(szSaveName, sharedStashFilename);
	strcat(szSaveName,".sss");
	log_msg("Shared file for saving : %s\n", szSaveName);

//	if (!MoveFileEx(szTempName, szSaveName, MOVEFILE_WRITE_THROUGH|MOVEFILE_REPLACE_EXISTING))
	DeleteFile(szSaveName);
	if (!MoveFile(szTempName, szSaveName))
		log_box("Could not create the shared save file.");
}

void saveSharedSaveFile(Unit* ptChar, BYTE** data, DWORD* maxSize, DWORD* curSize)
{
	*(DWORD *)(*data + *curSize) = FILE_SHAREDSTASH;
	*curSize += 4;
	if (PCPY->sharedGold)
	{
		*(WORD *)(*data + *curSize) = FILE_VERSION;
		*curSize += 2;

		*(DWORD *)(*data + *curSize) = PCPY->sharedGold;
		*curSize += 4;
	} else {
		*(WORD *)(*data + *curSize) = 0x3130;
		*curSize += 2;
	}
	saveStashList(ptChar, PCPY->sharedStash, data, maxSize, curSize);

	TCustomDll* currentDll = customDlls;
	while (currentDll)
	{
		currentDll->saveSharedSaveFile(ptChar, data, maxSize, curSize);
		currentDll=currentDll->nextDll;
	}
}


/*================================= END OF FILE =================================*/