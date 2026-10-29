/*=================================================================
	File created by Yohann NICOLAS.
	Add support 1.13d by L'Autour.
	Add support 1.14d by haxifix.

	Use a more big stash

=================================================================*/

#include "plugYFiles.h"		// Install_PlugYFiles()
#include "common.h"

bool active_bigStash = false;
bool active_bigStash_tested = false;
bool active_classicBigStash = false;

const char* tradeStash_RefFile= "%s\\TradeStash";
const BYTE gridX_BB = 10;
const BYTE gridY_BB = 10;
const DWORD gridLeft_BB = 16;
const DWORD gridRight_BB = 302;
const DWORD gridTop_BB = 82;
const DWORD gridbottom_BB = 371;

// Rows of inventory.txt : "Bank Page 1" is the stash of Classic games, "Big Bank Page 1" the stash of LoD games.
// The rows at +16 are the same pages in 800x600.
#define INV_BANK_PAGE		8
#define INV_BIG_BANK_PAGE	12
#define INV_HIGH_RES_SHIFT	16

void setBigStashGrid(InventoryBIN* inventory, DWORD shiftX, DWORD shiftY)
{
	inventory->gridX = gridX_BB;
	inventory->gridY = gridY_BB;
	inventory->gridLeft = gridLeft_BB + shiftX;
	inventory->gridRight = gridRight_BB + shiftX;
	inventory->gridTop = gridTop_BB + shiftY;
	inventory->gridBottom = gridbottom_BB + shiftY;
}

InventoryBIN* STDCALL modifStashGrid(InventoryBIN* ptInventoryBin)
{
	active_bigStash_tested = true;
	if ( onRealm || !active_bigStash) return ptInventoryBin;

	setBigStashGrid(ptInventoryBin + INV_BIG_BANK_PAGE, 0, 0);
	setBigStashGrid(ptInventoryBin + INV_BIG_BANK_PAGE + INV_HIGH_RES_SHIFT, 80, 60);
	if (active_classicBigStash)
	{
		setBigStashGrid(ptInventoryBin + INV_BANK_PAGE, 0, 0);
		setBigStashGrid(ptInventoryBin + INV_BANK_PAGE + INV_HIGH_RES_SHIFT, 80, 60);
	}

	return ptInventoryBin;
}

FCT_ASM ( caller_modifStashGrid )
	PUSH DWORD PTR SS:[ESP+0x14]
	PUSH DWORD PTR SS:[ESP+0x14]
	PUSH DWORD PTR SS:[ESP+0x14]
	PUSH DWORD PTR SS:[ESP+0x14]
	PUSH DWORD PTR SS:[ESP+0x14]
	CALL D2CompileTxtFile
	PUSH EAX
	CALL modifStashGrid
	RETN 0x14
}}

//FCT_ASM ( caller_modifStashGrid )
//	POP ESI
//	CALL D2CompileTxtFile
//	PUSH EAX
//	CALL modifStashGrid
//	JMP ESI
//}}

FCT_ASM ( caller_changeTradeStash )
	CMP onRealm,0
	JNZ normalTradeStash
	MOV EAX,modDataDirectory
	MOV DWORD PTR SS:[ESP+0x8],EAX
	MOV EAX,tradeStash_RefFile
	MOV DWORD PTR SS:[ESP+0x4],EAX
normalTradeStash:
	POP EAX
	PUSH 0x104
	JMP EAX
}}

/*
Diablo II Classic stash has its own layout : "bank" background, gold field above a 6x4 grid at the bottom.
With the big stash, Classic games use the LoD layout with the PlugY background :
the D2Client instructions choosing the stash layout read stashLodLayout instead of IsLodGame
(1.09b, 1.09d, 1.10 and 1.14d call a function returning IsLodGame : these calls are redirected to caller_isLodStashLayout).
*/
DWORD stashLodLayout = 0;
DWORD* ptStashBankImage = NULL;
DWORD D2isExpansionInstalled = 0;// Fog.#10227 (Game.exe function in 1.14d)

bool isLodStashLayout()
{
	return D2isLODGame() || (active_classicBigStash && !onRealm);
}

void STDCALL updateStashLayout()
{
	stashLodLayout = isLodStashLayout();
}

FCT_ASM ( caller_updateStashLayout )
	PUSH ECX
	PUSH EDX
	CALL updateStashLayout
	POP EDX
	POP ECX
	MOV EAX,ptStashBankImage
	MOV EAX,DWORD PTR DS:[EAX]
	RETN
}}

FCT_ASM ( caller_isOnStashGoldField )
	PUSH EAX
	PUSH EDX
	CALL updateStashLayout
	POP EDX
	POP EAX
	MOV ECX,stashLodLayout
	RETN
}}

FCT_ASM ( caller_isLodStashLayout )
	PUSH ECX
	PUSH EDX
	CALL updateStashLayout
	POP EDX
	POP ECX
	MOV EAX,stashLodLayout
	RETN
}}

// Load the PlugY background even if LoD isn't installed.
FCT_ASM ( caller_loadTradeStash )
	CMP onRealm,0
	JNZ normalLoadTradeStash
	MOV EAX,1
	RETN
normalLoadTradeStash:
	JMP D2isExpansionInstalled
}}


void Install_BigStash()
{
	static int isInstalled = false;
	if (isInstalled) return;

	Install_PlugYFiles();

	log_msg("Patch D2Common & D2Client for make 10x10 squares in the stash. (BigStash)\n");

	// modification of stash grid
	mem_seek R8(D2Common, C9F3, CA03, 14ED3, 5FCB5, 2A505, 1BDB5, 82CA5, 6CC25, 25C0F8);
	MEMC_REF4( D2CompileTxtFile, caller_modifStashGrid);
	//01B64ED2  |. E8 99AEFFFF    CALL D2Common.#10578
	//6FDAFCB4  |. E8 A7C3FCFF    CALL D2Common.#10653
	//6FD7A504  |. E8 5743FEFF    CALL D2Common.#10496                     ; \#10496
	//6FD6BDB4  |. E8 97600200    CALL D2Common.#10244                     ; \#10244
	//6FDD2CA4  |. E8 97C2FDFF    CALL D2Common.#10849                     ; \#10849
	//6FDBCC24  |. E8 B7FEF9FF    CALL D2Common.#10037                     ; \#10037
	//0065C0F7  |. E8 F461FBFF    CALL Game.006122F0                       ; \Game.006122F0

	// modification of stash background
	mem_seek R8(D2Client, 45B1C, 45B1C, 4C61C, A643C, 749BC, A9D7C, 8CC1C, 943FC, 89EB5);
	memt_byte( 0x68, 0xE8 );	// CALL caller_changeTradeStash
	MEMT_REF4( 0x00000104, caller_changeTradeStash);
	//6FAEC61C  |. 68 04010000    PUSH 104
	//6FB5643C  |. 68 04010000    PUSH 104
	//6FB249BC  |. 68 04010000    PUSH 104
	//6FB59D7C  |. 68 04010000    PUSH 104
	//6FB3CC1C  |. 68 04010000    PUSH 104
	//6FB443FC  |. 68 04010000    PUSH 104
	//00489EB5  |. 68 04010000    PUSH 104                                 ; |Arg2 = 00000104

	log_msg("Patch D2Client for use the LoD stash layout in Diablo II Classic games. (BigStash)\n");
	active_classicBigStash = true;
	D2isExpansionInstalled = R8(D2Client, C093A, BFCBA, CA6DA, BEC4, BED0, BF92, BF8C, BEA0, 8F20);

	// Load stash background even in Classic install
	mem_seek R8(D2Client, 45B06, 45B06, 4C606, A6426, 749A6, A9D66, 8CC06, 943E6, 89E9D);
	MEMC_REF4( D2isExpansionInstalled, caller_loadTradeStash);
	//6FAE5B05  |. E8 30AE0700    CALL <JMP.&Fog.#10227>
	//6FAE5B05  |. E8 B0A10700    CALL <JMP.&Fog.#10227>
	//6FAEC605  |. E8 D0E00700    CALL <JMP.&Fog.#10227>
	//6FB56425  |. E8 9A5AF6FF    CALL <JMP.&Fog.#10227>
	//6FB249A5  |. E8 2675F9FF    CALL <JMP.&Fog.#10227>
	//6FB59D65  |. E8 2822F6FF    CALL <JMP.&Fog.#10227>
	//6FB3CC05  |. E8 82F3F7FF    CALL <JMP.&Fog.#10227>
	//6FB443E5  |. E8 B67AF7FF    CALL <JMP.&Fog.#10227>
	//00489E9C  |. E8 7FF0F7FF    CALL Game.00408F20

	if (version_D2Client >= V111 && version_D2Client <= V113d)
	{
		ptStashBankImage = (DWORD*)R8(D2Client, 0000, 0000, 0000, 11BA7C, 11BC18, 11BA38, 11BC74, 11CB64, 0000);

		// Update stash layout before printing stash
		mem_seek R8(D2Client, 0000, 0000, 0000, B0E85, 7DE05, B34D5, 998D5, 9DCA5, 0000);
		memt_byte( 0xA1, 0xE8 );
		MEMT_REF4( ptStashBankImage, caller_updateStashLayout);
		//6FB60E85  |. A1 7CBABC6F    MOV EAX,DWORD PTR DS:[6FBCBA7C]
		//6FB2DE05  |. A1 18BCBC6F    MOV EAX,DWORD PTR DS:[6FBCBC18]
		//6FB634D5  |. A1 38BABC6F    MOV EAX,DWORD PTR DS:[6FBCBA38]
		//6FB498D5  |. A1 74BCBC6F    MOV EAX,DWORD PTR DS:[6FBCBC74]
		//6FB4DCA5  |. A1 64CBBC6F    MOV EAX,DWORD PTR DS:[6FBCCB64]

		// Stash background
		mem_seek R8(D2Client, 0000, 0000, 0000, B0E94, 7DE14, B34E4, 998E4, 9DCB4, 0000);
		MEMT_DWORD( ptIsLodGame, &stashLodLayout);
		//6FB60E92  |. 392D F4B3BA6F  CMP DWORD PTR DS:[6FBAB3F4],EBP
		//6FB2DE12  |. 392D F4A2BC6F  CMP DWORD PTR DS:[6FBCA2F4],EBP
		//6FB634E2  |. 392D 0C33BB6F  CMP DWORD PTR DS:[6FBB330C],EBP
		//6FB498E2  |. 392D 5498BC6F  CMP DWORD PTR DS:[6FBC9854],EBP
		//6FB4DCB2  |. 392D B487BB6F  CMP DWORD PTR DS:[6FBB87B4],EBP

		// Position of max gold text
		mem_seek R8(D2Client, 0000, 0000, 0000, B0FAB, 7DF2B, B35FB, 999FB, 9DDCB, 0000);
		MEMT_DWORD( ptIsLodGame, &stashLodLayout);
		//6FB60FAA  |. A1 F4B3BA6F    MOV EAX,DWORD PTR DS:[6FBAB3F4]
		//6FB2DF2A  |. A1 F4A2BC6F    MOV EAX,DWORD PTR DS:[6FBCA2F4]
		//6FB635FA  |. A1 0C33BB6F    MOV EAX,DWORD PTR DS:[6FBB330C]
		//6FB499FA  |. A1 5498BC6F    MOV EAX,DWORD PTR DS:[6FBC9854]
		//6FB4DDCA  |. A1 B487BB6F    MOV EAX,DWORD PTR DS:[6FBB87B4]

		// Position of gold field
		mem_seek R8(D2Client, 0000, 0000, 0000, A6AAC, 7164C, A69FC, 8D14C, 9119C, 0000);
		MEMT_DWORD( ptIsLodGame, &stashLodLayout);
		//6FB56AAA  |. 8B3D F4B3BA6F  MOV EDI,DWORD PTR DS:[6FBAB3F4]
		//6FB2164A  |. 8B3D F4A2BC6F  MOV EDI,DWORD PTR DS:[6FBCA2F4]
		//6FB569FA  |. 8B3D 0C33BB6F  MOV EDI,DWORD PTR DS:[6FBB330C]
		//6FB3D14A  |. 8B3D 5498BC6F  MOV EDI,DWORD PTR DS:[6FBC9854]
		//6FB4119A  |. 8B3D B487BB6F  MOV EDI,DWORD PTR DS:[6FBB87B4]

		// Mouse over gold field
		mem_seek R8(D2Client, 0000, 0000, 0000, A4CA0, 6FE60, A5530, 8B6B0, 8FA30, 0000);
		memt_byte( 0x8B, 0xE8 );
		MEMT_REF4( 0x0D | ((DWORD)ptIsLodGame << 8), caller_isOnStashGoldField);
		memt_byte( (BYTE)((DWORD)ptIsLodGame >> 24), 0x90 );
		//6FB54CA0  |$ 8B0D F4B3BA6F  MOV ECX,DWORD PTR DS:[6FBAB3F4]
		//6FB1FE60  |$ 8B0D F4A2BC6F  MOV ECX,DWORD PTR DS:[6FBCA2F4]
		//6FB55530  |$ 8B0D 0C33BB6F  MOV ECX,DWORD PTR DS:[6FBB330C]
		//6FB3B6B0  |$ 8B0D 5498BC6F  MOV ECX,DWORD PTR DS:[6FBC9854]
		//6FB3FA30  |$ 8B0D B487BB6F  MOV ECX,DWORD PTR DS:[6FBB87B4]
	}
	else
	{
		// 1.09b, 1.09d, 1.10 and 1.14d
		DWORD D2isLODGameFct = R8(D2Client, BA00, B9F0, C080, 0000, 0000, 0000, 0000, 0000, 4DCC0);

		// Stash background
		mem_seek R8(D2Client, 38ECF, 38ECF, 3F1FE, 0000, 0000, 0000, 0000, 0000, 8F19C);
		MEMC_REF4( D2isLODGameFct, caller_isLodStashLayout);
		//6FAD8ECE  |. E8 2D2BFDFF    CALL D2Client.6FAABA00
		//6FAD8ECE  |. E8 1D2BFDFF    CALL D2Client.6FAAB9F0
		//6FADF1FD  |. E8 7ECEFCFF    CALL D2Client.6FAAC080
		//0048F19B  |. E8 20EBFBFF    CALL Game.0044DCC0

		// Position of max gold text
		mem_seek R8(D2Client, 38FF1, 38FF1, 3F327, 0000, 0000, 0000, 0000, 0000, 8F2B1);
		MEMC_REF4( D2isLODGameFct, caller_isLodStashLayout);
		//6FAD8FF0  |. E8 0B2AFDFF    CALL D2Client.6FAABA00
		//6FAD8FF0  |. E8 FB29FDFF    CALL D2Client.6FAAB9F0
		//6FADF326  |. E8 55CDFCFF    CALL D2Client.6FAAC080
		//0048F2B0  |. E8 0BEAFBFF    CALL Game.0044DCC0

		// Position of gold field
		mem_seek R8(D2Client, 42D46, 42D46, 498F8, 0000, 0000, 0000, 0000, 0000, 8820D);
		MEMC_REF4( D2isLODGameFct, caller_isLodStashLayout);
		//6FAE2D45  |. E8 B68CFCFF    CALL D2Client.6FAABA00
		//6FAE2D45  |. E8 A68CFCFF    CALL D2Client.6FAAB9F0
		//6FAE98F7  |. E8 8427FCFF    CALL D2Client.6FAAC080
		//0048820C  |. E8 AF5AFCFF    CALL Game.0044DCC0

		// Mouse over gold field
		mem_seek R8(D2Client, 45052, 45052, 4BB62, 0000, 0000, 0000, 0000, 0000, 89921);
		MEMC_REF4( D2isLODGameFct, caller_isLodStashLayout);
		//6FAE5051  |. E8 AA69FCFF    CALL D2Client.6FAABA00
		//6FAE5051  |. E8 9A69FCFF    CALL D2Client.6FAAB9F0
		//6FAEBB61  |. E8 1A05FCFF    CALL D2Client.6FAAC080
		//00489920  |$ E8 9B43FCFF    CALL Game.0044DCC0

		if (version_D2Client <= V110)
		{
			// 1.09 and 1.10 check the gold field again : popup of the gold field, button down (second check) and button up.
			// Popup of gold field
			mem_seek R8(D2Client, 42D9E, 42D9E, 49950, 0000, 0000, 0000, 0000, 0000, 0000);
			MEMC_REF4( D2isLODGameFct, caller_isLodStashLayout);
			//6FAE2D9D  |. E8 5E8CFCFF    CALL D2Client.6FAABA00
			//6FAE2D9D  |. E8 4E8CFCFF    CALL D2Client.6FAAB9F0
			//6FAE994F  |. E8 2C27FCFF    CALL D2Client.6FAAC080

			// Mouse over gold field (button down, second check)
			mem_seek R8(D2Client, 45341, 45341, 4BE4E, 0000, 0000, 0000, 0000, 0000, 0000);
			MEMC_REF4( D2isLODGameFct, caller_isLodStashLayout);
			//6FAE5340  |. E8 BB66FCFF    CALL D2Client.6FAABA00
			//6FAE5340  |. E8 AB66FCFF    CALL D2Client.6FAAB9F0
			//6FAEBE4D  |. E8 2E02FCFF    CALL D2Client.6FAAC080

			// Mouse over gold field (button up)
			mem_seek R8(D2Client, 4577E, 4577E, 4C27B, 0000, 0000, 0000, 0000, 0000, 0000);
			MEMC_REF4( D2isLODGameFct, caller_isLodStashLayout);
			//6FAE577D  |. E8 7E62FCFF    CALL D2Client.6FAABA00
			//6FAE577D  |. E8 6E62FCFF    CALL D2Client.6FAAB9F0
			//6FAEC27A  |. E8 01FEFBFF    CALL D2Client.6FAAC080
		}
	}

	log_msg("\n");

	isInstalled = true;
}

/*================================= END OF FILE =================================*/