/*=================================================================
	Stash sort button.

	Sort the items of all the pages of the stash shown (personal or shared) :
	the order and the places come from sortStashCore.h.
	The server and the client both compute the sort and move their own items. The server sends one message
	(UC_SORT_STASH) with a checksum of its result, the client moves its items only when its result is the same.
	The names and the index flags of the pages are removed.

	Only with the big stash in 1.09b, 1.09d, 1.13c, 1.13d and 1.14d (10x10 stash in LoD and Classic games).

=================================================================*/

#include "sortStash.h"
#include "sortStashCore.h"
#include "infinityStash.h"
#include "bigStash.h"
#include "updateClient.h"
#include "common.h"

bool active_sortStash = false;

// Grid of the big stash (gridX_BB and gridY_BB in BigStash.cpp).
#define SORT_GRID_X 10
#define SORT_GRID_Y 10


/****************************** Item data ******************************/

/*
1.09 (1.09b and 1.09d) : the lists of the inventory and of the stash pages link nodes (D2GetRealItem gives the item
of a node, D2UnitGetNextItem the next node), and the structures aren't the ones of D2UnitStruct.h and d2BinFile.h :
the 1.09 offsets come from the D2Common functions and from the fields tables of the txt files.
*/
#define IS_109 (version_D2Common <= V109d)

// Unit id (GUID) of an item, the same on the server and the client : Unit+0C (nItemNum in D2UnitStruct.h,
// dwUnitId of D2UnitStrc in D2MOO). Unit+08 (nUnitId) isn't it, except in 1.09 where the unit id is Unit+08.
#define UNIT_GUID(U) (IS_109 ? (U)->nUnitId : (U)->nItemNum)

// Field of items.txt in 1.09 (ItemsBIN is the record of 1.10 and after).
#define ITEMSBIN_109(B, T, O) (*(T*)((BYTE*)(B) + (O)))

static Unit* getRealItem(Unit* ptItem)
{
	return IS_109 ? D2GetRealItem(ptItem) : ptItem;
}

// ItemData : Unit+70 in 1.09. Same offsets for the quality and the flags.
static ItemData* getItemData(Unit* ptItem)
{
	return *(ItemData**)((DWORD)ptItem + shifting.ptSpecificData);
}

// Inventory of the item (socketed items) : Unit+84 in 1.09.
static Inventory* getItemInventory(Unit* ptItem)
{
	return *(Inventory**)((DWORD)ptItem + shifting.ptInventory);
}

static void codeToString(DWORD code, char* str)
{
	int len = 0;
	for (int i = 0; i < 4; i++)
	{
		char c = (char)((code >> (i * 8)) & 0xFF);
		if (!c) break;
		str[len++] = c;
	}
	while (len && str[len - 1] == ' ')
		len--;
	str[len] = 0;
}

enum
{
	T_WEAP, T_ARMO, T_SHLD, T_HELM, T_TORS, T_GLOV, T_BOOT, T_BELT,
	T_AMAZ, T_ASSN, T_BARB, T_DRUI, T_NECR, T_PALA, T_SORC, T_WARL,
	T_NB
};
static const char* const typeCodes[T_NB] =
{
	"weap", "armo", "shld", "helm", "tors", "glov", "boot", "belt",
	"amaz", "assn", "barb", "drui", "necr", "pala", "sorc", "warl"
};
static int typeIndex[T_NB];
static bool typeIndexLoaded = false;

// Rows of ItemTypes.txt used by the sort, found by their code (they can move in a mod).
static void loadTypeIndex()
{
	if (typeIndexLoaded) return;
	for (int t = 0; t < T_NB; t++)
	{
		typeIndex[t] = -1;
		DWORD nbItemTypes = IS_109 ? *D2GetNbItemTypesBIN() : SgptDataTables->nbItemTypes;
		for (DWORD i = 0; i < nbItemTypes; i++)
		{
			ItemTypesBIN* itemType = D2GetItemTypesBIN(i);
			char code[5];
			codeToString(itemType ? itemType->code : 0, code);
			if (!strcmp(code, typeCodes[t]))
			{
				typeIndex[t] = i;
				break;
			}
		}
		log_msg("sortStash : item type %s = %d\n", typeCodes[t], typeIndex[t]);
	}
	typeIndexLoaded = true;
}

static bool isType(Unit* ptItem, int t)
{
	return typeIndex[t] >= 0 && D2CheckItemType(ptItem, typeIndex[t]);
}

#define SORT_NO_RUNEWORD 0x7FFFFFFF

// Row of runes.txt : RunesBIN, or in 1.09 a record of 0x118 bytes with bytes for the item types.
struct RuneRow
{
	bool	complete;
	int		itypes[6];
	int		etypes[3];
	int		runes[6];
};

static void getRuneRow(BYTE* first, int i, RuneRow* row)
{
	if (IS_109)
	{
		BYTE* record = first + i * 0x118;
		row->complete = record[0x80] != 0;
		for (int t = 0; t < 6; t++)
			row->itypes[t] = record[0x86 + t];
		for (int t = 0; t < 3; t++)
			row->etypes[t] = record[0x8C + t];
		for (int r = 0; r < 6; r++)
			row->runes[r] = ((int*)(record + 0x90))[r];
	}
	else
	{
		RunesBIN* runeword = (RunesBIN*)first + i;
		row->complete = runeword->Complete != 0;
		for (int t = 0; t < 6; t++)
			row->itypes[t] = runeword->Itypes[t];
		for (int t = 0; t < 3; t++)
			row->etypes[t] = runeword->Etypes[t];
		for (int r = 0; r < 6; r++)
			row->runes[r] = runeword->Runes[r];
	}
}

/*
Row of runes.txt of a runeword item, found like ITEMS_GetRunesTxtRecordFromItem of D2Common (#10822 in 1.10, see D2MOO) :
the socketed runes match the runes of the row (in the same order) and the type of the item is allowed by the row.
*/
static int getRunewordRow(Unit* ptItem)
{
	Inventory* ptSockets = getItemInventory(ptItem);
	if (!getItemData(ptItem)->isRuneword || !ptSockets)
		return SORT_NO_RUNEWORD;

	int socketed[6] = {0, 0, 0, 0, 0, 0};
	int nbSocketed = 0;
	for (Unit* ptRune = D2InventoryGetFirstItem(ptSockets); ptRune && nbSocketed < 6; ptRune = D2UnitGetNextItem(ptRune))
		socketed[nbSocketed++] = getRealItem(ptRune)->nTxtFileNo;

	int nbRunewords = *D2GetNbRunesBIN();
	BYTE* runesFirst;
	if (IS_109)
		runesFirst = *(BYTE**)(D2GetNbRunesBIN() + 1);	// the table follows the number of rows
	else
		runesFirst = nbRunewords > 1 ? (BYTE*)(D2GetRunesBIN(1) - 1) : NULL;	// like NewInterface_Runewords.cpp
	for (int i = 0; runesFirst && i < nbRunewords; i++)
	{
		RuneRow runeword;
		getRuneRow(runesFirst, i, &runeword);
		if (!runeword.complete)
			continue;

		bool match = true;
		for (int r = 0; match && r < 6 && runeword.runes[r] > 0; r++)
			if (nbSocketed < r || socketed[r] != runeword.runes[r])
				match = false;
		if (!match)
			continue;

		bool excluded = false;
		for (int t = 0; !excluded && t < 3 && runeword.etypes[t]; t++)
			excluded = D2CheckItemType(ptItem, runeword.etypes[t]) != 0;
		if (excluded)
			continue;

		for (int t = 0; t < 6 && runeword.itypes[t]; t++)
			if (D2CheckItemType(ptItem, runeword.itypes[t]))
				return i;
	}
	return SORT_NO_RUNEWORD;
}

// ptUnit : item of the stash list (a node in 1.09).
static void fillSortItem(SortItem* item, Unit* ptUnit)
{
	Unit* ptItem = getRealItem(ptUnit);
	ItemsBIN* itemsBIN = D2GetItemsBIN(ptItem->nTxtFileNo);
	ItemData* data = getItemData(ptItem);

	DWORD itemCode, uberCode, hellCode, type;
	ZeroMemory(item, sizeof(SortItem));
	if (IS_109)
	{
		itemCode = ITEMSBIN_109(itemsBIN, DWORD, 0x144);
		uberCode = ITEMSBIN_109(itemsBIN, DWORD, 0x14C);
		hellCode = ITEMSBIN_109(itemsBIN, DWORD, 0x150);	// ultracode
		type = ITEMSBIN_109(itemsBIN, BYTE, 0x1B3);
		item->width = ITEMSBIN_109(itemsBIN, BYTE, 0x1A4);
		item->height = ITEMSBIN_109(itemsBIN, BYTE, 0x1A5);
	}
	else
	{
		itemCode = itemsBIN->ItemCode;
		uberCode = itemsBIN->UberCode;
		hellCode = itemsBIN->HellCode;
		type = itemsBIN->Type;
		item->width = itemsBIN->InvWidth;
		item->height = itemsBIN->InvHeight;
	}

	item->unit = ptUnit;
	item->unitId = UNIT_GUID(ptItem);
	codeToString(itemCode, item->code);
	ItemTypesBIN* itemType = D2GetItemTypesBIN(type);
	codeToString(itemType ? itemType->code : 0, item->itype);
	item->quality = data->quality;
	item->isRuneword = data->isRuneword != 0;
	item->isEthereal = data->isEtheral != 0;
	item->isWeapon = isType(ptItem, T_WEAP);
	item->isArmor = isType(ptItem, T_ARMO);

	// getBodyPosOrder of sort_stash.js (shield before helm).
	if (isType(ptItem, T_SHLD))			item->bodyPos = 3;
	else if (isType(ptItem, T_HELM))	item->bodyPos = 0;
	else if (isType(ptItem, T_TORS))	item->bodyPos = 2;
	else if (isType(ptItem, T_GLOV))	item->bodyPos = 4;
	else if (isType(ptItem, T_BOOT))	item->bodyPos = 5;
	else if (isType(ptItem, T_BELT))	item->bodyPos = 6;
	else if (item->isWeapon)			item->bodyPos = 7;
	else								item->bodyPos = 8;

	// getSpecialRank : class items, then circlets.
	for (int t = T_AMAZ; t <= T_WARL; t++)
		if (isType(ptItem, t))
			item->specialRank = 2;
	if (!item->specialRank && !strcmp(item->itype, "circ"))
		item->specialRank = 1;

	if (itemCode == hellCode)		item->tier = 2;
	else if (itemCode == uberCode)	item->tier = 1;

	// 1.09 : a row of setitems.txt is a set (no row per set item), the unique id of a set item is the row of its set.
	DWORD uniqueID = IS_109 ? (DWORD)D2GetUniqueID(ptItem) : data->uniqueID;
	if (data->quality == ITEMQUALITY_SET && IS_109)
		item->setKey = uniqueID;
	else if (data->quality == ITEMQUALITY_SET && uniqueID < SgptDataTables->nbSetItems)
		item->setKey = SgptDataTables->setItems[uniqueID].setId;
	if (data->quality == ITEMQUALITY_SET || data->quality == ITEMQUALITY_UNIQUE)
		item->row = uniqueID;
	item->runeword = getRunewordRow(ptItem);
	item->picture = IS_109 ? D2GetVarGfxIndex(ptItem) : data->varGfx;
}


/****************************** Pages ******************************/

static Stash* firstPage(Unit* ptChar, bool isShared)
{
	return isShared ? PCPY->sharedStash : PCPY->selfStash;
}

// Items of all the pages : the current page is in the inventory, the others in their list.
static int getStashItems(Unit* ptChar, Stash* first, Unit*** items)
{
	int nb = 0, size = 0;
	*items = NULL;
	for (Stash* stash = first; stash; stash = stash->nextStash)
	{
		Unit* ptItem = stash == PCPY->currentStash ? D2InventoryGetFirstItem(PCInventory) : stash->ptListItem;
		for (; ptItem; ptItem = D2UnitGetNextItem(ptItem))
		{
			if (stash == PCPY->currentStash && D2ItemGetPage(getRealItem(ptItem)) != 4)
				continue;
			if (nb == size)
			{
				size = size ? size * 2 : 128;
				*items = (Unit**)realloc(*items, size * sizeof(Unit*));
				d2_assert(!*items, "Error : Memory allocation", __FILE__, __LINE__);
			}
			(*items)[nb++] = ptItem;
		}
	}
	return nb;
}

/*
1.09 (like changeToSelectedStash_9) : the nodes of the current page are taken out of the list of the inventory
and their squares are freed with D2Common10250. D2InvAddItem only puts an item in the squares, D2Common10242
adds a new node in the list of the inventory. The other pages are lists of nodes.
*/
static void moveStashItems_9(Unit* ptChar, Stash* first, Stash** pages, Unit** items, SortPlace* places, int nb, bool isClient)
{
	Stash* current = PCPY->currentStash;
	Inventory* ptInventory = PCInventory;

	Unit* ptNextItem;
	Unit* ptPrevItem = NULL;
	Unit* ptItem = D2InventoryGetFirstItem(ptInventory);
	while (ptItem)
	{
		ptNextItem = D2UnitGetNextItem(ptItem);
		if (D2ItemGetPage(D2GetRealItem(ptItem)) == 4)
		{
			D2SetAnim(D2GetRealItem(ptItem),-1);
			if (ptPrevItem)
				ptPrevItem->mode = (DWORD)ptNextItem;//is ptPrevItem->nextNode = ptNextItem;
			else
				ptInventory->currentUsedSocket = (DWORD)ptNextItem;//is ptInventory->ptListItem = ptNextItem;
			if (!ptNextItem)
				ptInventory->Inventory2C = (DWORD)ptPrevItem;
			ptInventory->Inventory30 = ptInventory->Inventory30 - 1;
			D2Common10250(__FILE__,__LINE__,ptInventory, D2GetPosX(D2GetRealItem(ptItem)), D2GetPosY(D2GetRealItem(ptItem)), 0xC, isClient, 4);
		}
		else
			ptPrevItem = ptItem;
		ptItem = ptNextItem;
	}
	for (Stash* stash = first; stash; stash = stash->nextStash)
		stash->ptListItem = NULL;

	for (int i = 0; i < nb; i++)
	{
		Unit* ptNode = items[i];
		ptItem = D2GetRealItem(ptNode);
		Stash* stash = pages[places[i].page];
		if (stash == current)
		{
			if (D2InvAddItem(ptInventory, ptItem, places[i].x, places[i].y, 0xC, isClient, 4))
				D2Common10242(ptInventory, ptItem, 1);
			else
				log_msg("sortStash : can't put item %d at %d,%d of the current page\n", UNIT_GUID(ptItem), places[i].x, places[i].y);
		}
		else
		{
			D2SetPosX(ptItem, places[i].x);
			D2SetPosY(ptItem, places[i].y);
			ptNode->mode = (DWORD)stash->ptListItem;//is ptNode->nextNode = ptListItem
			stash->ptListItem = ptNode;
		}
	}
}

// Put each item at its place. The pages missing are added at the end of the stash.
static void moveStashItems(Unit* ptChar, Stash* first, Unit** items, SortPlace* places, int nb, int nbPages, bool isClient)
{
	Stash* current = PCPY->currentStash;

	Stash** pages = (Stash**)malloc((nbPages + 1) * sizeof(Stash*));
	d2_assert(!pages, "Error : Memory allocation", __FILE__, __LINE__);
	Stash* stash = first;
	for (int i = 0; i < nbPages; i++)
	{
		if (!stash)
			stash = addStash(ptChar, first->isShared, false, pages[i - 1]);
		pages[i] = stash;
		stash = stash->nextStash;
	}

	if (IS_109)
		moveStashItems_9(ptChar, first, pages, items, places, nb, isClient);
	else
	{
		// Take the items of the current page out of the inventory (like changeToSelectedStash).
		Unit* ptNextItem;
		Unit* ptItem = D2InventoryGetFirstItem(PCInventory);
		while (ptItem)
		{
			ptNextItem = D2UnitGetNextItem(ptItem);
			if (D2ItemGetPage(ptItem) == 4)
			{
				BYTE tmp = ptItem->ptItemData->ItemData2;
				Unit* ptRemoved = D2InvRemoveItem(PCInventory, ptItem);
				ptRemoved->ptItemData->ItemData2 = tmp;
				if (ptRemoved != ptItem)
					for (int i = 0; i < nb; i++)
						if (items[i] == ptItem)
							items[i] = ptRemoved;
			}
			ptItem = ptNextItem;
		}
		for (stash = first; stash; stash = stash->nextStash)
			stash->ptListItem = NULL;

		for (int i = 0; i < nb; i++)
		{
			ptItem = items[i];
			stash = pages[places[i].page];
			ptItem->path->x = places[i].x;
			ptItem->path->y = places[i].y;
			if (stash == current)
			{
				ptItem->ptItemData->ptNextItem = NULL;
				if (!D2InvAddItem(PCInventory, ptItem, places[i].x, places[i].y, 0xC, isClient, 4))
					log_msg("sortStash : can't put item %d at %d,%d of the current page\n", UNIT_GUID(ptItem), places[i].x, places[i].y);
			}
			else
			{
				ptItem->ptItemData->ptNextItem = stash->ptListItem;
				stash->ptListItem = ptItem;
			}
		}
	}
	free(pages);

	for (stash = first; stash; stash = stash->nextStash)
	{
		if (stash->name)
		{
			free(stash->name);
			stash->name = NULL;
		}
		stash->isIndex = 0;
		stash->isMainIndex = 0;
	}
}


/****************************** Sort ******************************/

/*
Both the server and the client compute the sort with their own copy of the items : the result only depends
on the item data and the unit ids, the same on both sides. The server only sends a checksum of its result
(a stash can have thousands of items : one message per item was too much for the network in TCP/IP).
*/
struct StashSort
{
	Stash*		first;
	Unit**		units;
	SortItem*	items;
	SortPlace*	places;
	int			nb;
	int			nbPages;
	bool		compact;
};

static void freeStashSort(StashSort* sort)
{
	free(sort->units);
	free(sort->items);
	free(sort->places);
}

// force : -1 to choose the packing with maxPages, 0 normal packing, 1 compact packing.
static void planStashSort(Unit* ptChar, bool isShared, DWORD maxPages, int force, StashSort* sort)
{
	loadTypeIndex();
	sort->first = firstPage(ptChar, isShared);
	sort->nb = getStashItems(ptChar, sort->first, &sort->units);
	sort->items = (SortItem*)malloc((sort->nb + 1) * sizeof(SortItem));
	sort->places = (SortPlace*)malloc((sort->nb + 1) * sizeof(SortPlace));
	d2_assert(!sort->items || !sort->places, "Error : Memory allocation", __FILE__, __LINE__);
	for (int i = 0; i < sort->nb; i++)
		fillSortItem(&sort->items[i], sort->units[i]);
	sort->nbPages = sortStashPlan(sort->items, sort->nb, SORT_GRID_X, SORT_GRID_Y, maxPages, force, sort->places, &sort->compact);
}


/****************************** Server ******************************/

void sortStash(Unit* ptChar)
{
	if (!active_sortStash || !PCPY->currentStash)
		return;

	bool isShared = PCPY->currentStash->isShared != 0;
	// Number of pages allowed (MaxPersonnalPages / MaxSharedPages), 0 : no limit.
	DWORD maxPages = (isShared ? maxSharedPages : maxSelfPages) + 1;
	StashSort sort;
	planStashSort(ptChar, isShared, maxPages, -1, &sort);
	log_msg("sortStash : %d items of the %s stash -> %d pages%s\n", sort.nb, isShared ? "shared" : "personal", sort.nbPages, sort.compact ? " (compact)" : "");

	if (sort.nbPages >= 0)
	{
		DWORD checksum = sortStashChecksum(sort.items, sort.places, sort.nb, sort.nbPages);
		moveStashItems(ptChar, sort.first, sort.units, sort.places, sort.nb, sort.nbPages, false);
		updateClient(ptChar, UC_SORT_STASH, (isShared ? 1 : 0) | (sort.compact ? 2 : 0), sort.nb, checksum);
	}
	else
		log_msg("sortStash : not enough pages, nothing moved\n");

	freeStashSort(&sort);
}


/****************************** Client ******************************/

void sortStashClient(DWORD flags, DWORD nbItems, DWORD checksum)
{
	Unit* ptChar = D2GetClientPlayer();
	if (!ptChar || !PCPY->currentStash)
		return;

	bool isShared = (flags & 1) != 0;
	StashSort sort;
	planStashSort(ptChar, isShared, 0, (flags & 2) ? 1 : 0, &sort);
	DWORD clientChecksum = sort.nbPages >= 0 ? sortStashChecksum(sort.items, sort.places, sort.nb, sort.nbPages) : 0;
	log_msg("sortStashClient : %d items -> %d pages, checksum %08X (server : %d items, checksum %08X)\n", sort.nb, sort.nbPages, clientChecksum, nbItems, checksum);

	if (sort.nbPages >= 0 && sort.nb == (int)nbItems && clientChecksum == checksum)
		moveStashItems(ptChar, sort.first, sort.units, sort.places, sort.nb, sort.nbPages, true);
	else
		log_msg("sortStashClient : the sort of the client isn't the sort of the server, nothing moved\n");

	freeStashSort(&sort);
}


/****************************** Install ******************************/

void Install_SortStash()
{
	static int isInstalled = false;
	if (isInstalled) return;

	// The 10x10 grid of the big stash is used in all games only where Classic games have it too.
	active_sortStash = active_bigStash && active_classicBigStash;
	log_msg("Stash sort button : %s\n\n", active_sortStash ? "on" : "off (needs ActiveBigStash=1 with 1.09b, 1.09d, 1.13c, 1.13d or 1.14d)");

	isInstalled = true;
}

/*================================= END OF FILE =================================*/