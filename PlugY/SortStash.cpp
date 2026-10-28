/*=================================================================
	Stash sort button.

	Sort the items of all the pages of the stash shown (personal or shared) :
	the order and the places come from sortStashCore.h.
	The server and the client both compute the sort and move their own items. The server sends one message
	(UC_SORT_STASH) with a checksum of its result, the client moves its items only when its result is the same.
	The names and the index flags of the pages are removed.

	Only with the big stash in 1.13c, 1.13d and 1.14d (10x10 stash in LoD and Classic games).

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

// Unit id (GUID) of an item, the same on the server and the client : Unit+0C (nItemNum in D2UnitStruct.h,
// dwUnitId of D2UnitStrc in D2MOO). Unit+08 (nUnitId) isn't it.
#define UNIT_GUID(U) ((U)->nItemNum)

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
		for (DWORD i = 0; i < SgptDataTables->nbItemTypes; i++)
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

/*
Row of runes.txt of a runeword item, found like ITEMS_GetRunesTxtRecordFromItem of D2Common (#10822 in 1.10, see D2MOO) :
the socketed runes match the runes of the row (in the same order) and the type of the item is allowed by the row.
*/
static int getRunewordRow(Unit* ptItem)
{
	if (!ptItem->ptItemData->isRuneword || !ptItem->ptInventory)
		return SORT_NO_RUNEWORD;

	int socketed[6] = {0, 0, 0, 0, 0, 0};
	int nbSocketed = 0;
	for (Unit* ptRune = D2InventoryGetFirstItem(ptItem->ptInventory); ptRune && nbSocketed < 6; ptRune = D2UnitGetNextItem(ptRune))
		socketed[nbSocketed++] = ptRune->nTxtFileNo;

	int nbRunewords = *D2GetNbRunesBIN();
	RunesBIN* runesFirst = nbRunewords > 1 ? D2GetRunesBIN(1) - 1 : NULL;	// like NewInterface_Runewords.cpp
	for (int i = 0; runesFirst && i < nbRunewords; i++)
	{
		RunesBIN* runeword = runesFirst + i;
		if (!runeword->Complete)
			continue;

		bool match = true;
		for (int r = 0; match && r < 6 && runeword->Runes[r] > 0; r++)
			if (nbSocketed < r || socketed[r] != runeword->Runes[r])
				match = false;
		if (!match)
			continue;

		bool excluded = false;
		for (int t = 0; !excluded && t < 3 && runeword->Etypes[t]; t++)
			excluded = D2CheckItemType(ptItem, runeword->Etypes[t]) != 0;
		if (excluded)
			continue;

		for (int t = 0; t < 6 && runeword->Itypes[t]; t++)
			if (D2CheckItemType(ptItem, runeword->Itypes[t]))
				return i;
	}
	return SORT_NO_RUNEWORD;
}

static void fillSortItem(SortItem* item, Unit* ptItem)
{
	ItemsBIN* itemsBIN = D2GetItemsBIN(ptItem->nTxtFileNo);
	ItemData* data = ptItem->ptItemData;

	ZeroMemory(item, sizeof(SortItem));
	item->unit = ptItem;
	item->unitId = UNIT_GUID(ptItem);
	codeToString(itemsBIN->ItemCode, item->code);
	ItemTypesBIN* itemType = D2GetItemTypesBIN(itemsBIN->Type);
	codeToString(itemType ? itemType->code : 0, item->itype);
	item->width = itemsBIN->InvWidth;
	item->height = itemsBIN->InvHeight;
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

	if (itemsBIN->ItemCode == itemsBIN->HellCode)		item->tier = 2;
	else if (itemsBIN->ItemCode == itemsBIN->UberCode)	item->tier = 1;

	if (data->quality == ITEMQUALITY_SET && data->uniqueID < SgptDataTables->nbSetItems)
		item->setKey = SgptDataTables->setItems[data->uniqueID].setId;
	if (data->quality == ITEMQUALITY_SET || data->quality == ITEMQUALITY_UNIQUE)
		item->row = data->uniqueID;
	item->runeword = getRunewordRow(ptItem);
	item->picture = data->varGfx;
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
			if (stash == PCPY->currentStash && D2ItemGetPage(ptItem) != 4)
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
	log_msg("Stash sort button : %s\n\n", active_sortStash ? "on" : "off (needs ActiveBigStash=1 with 1.13c, 1.13d or 1.14d)");

	isInstalled = true;
}

/*================================= END OF FILE =================================*/