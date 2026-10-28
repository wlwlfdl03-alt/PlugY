/*=================================================================
	Stash sort : order of the items and their place in the pages.

	Port of sortPlugyAllPages (sort_stash.js) : compareSortableItems + packSortedItems.
	No game function here : the item data are read by SortStash.cpp,
	so this file is also built on Linux by tools/sort_stash_test.cpp.

	Differences with sort_stash.js :
	- runewords are grouped by runeword (row in runes.txt, a new page for each one) before the other keys,
	- key 13 (display name) is the item code then the unique/set row,
	- the sets are ordered by their row in sets.txt, not by name,
	- equal items keep the order of their unit id.

=================================================================*/
#pragma once

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

struct SortItem
{
	// Read from the game.
	void*	unit;
	unsigned int unitId;
	char	code[5];			// item code, without the trailing spaces
	char	itype[5];			// code of the item type (ItemTypes.txt)
	int		width;
	int		height;
	int		quality;			// 1 low .. 9 tempered
	bool	isRuneword;
	bool	isEthereal;
	bool	isWeapon;			// item type equivalent to "weap"
	bool	isArmor;			// item type equivalent to "armo"
	int		bodyPos;			// 0 helm, 2 body, 3 shield, 4 gloves, 5 boots, 6 belt, 7 weapon, 8 other
	int		specialRank;		// 0 normal, 1 circlet, 2 class item
	int		tier;				// 0 normal, 1 exceptional, 2 elite
	int		setKey;				// row of the set in sets.txt (set items)
	int		row;				// row in uniqueitems.txt / setitems.txt (unique and set items), else 0
	int		runeword;			// row in runes.txt (runeword items)
	int		picture;			// picture variant of the item (rings, amulets, charms, jewels)

	// Computed by sortStashPrepare().
	int		sortQuality;
	const char* category;
	const char* miscSub;
	int		classOrder;
	int		miscSubOrder;
	int		innerOrder;
	char	groupItem[12];		// key 11 : "itype|code" for Misc
};

struct SortPlace
{
	int page;
	int x;
	int y;
};

#define SORT_QUALITY_NORMAL		0
#define SORT_QUALITY_MAGIC		1
#define SORT_QUALITY_RARE		2
#define SORT_QUALITY_CRAFTED	3
#define SORT_QUALITY_RUNEWORD	4
#define SORT_QUALITY_SET		5
#define SORT_QUALITY_UNIQUE		6

#define SORT_PAGE_SAFETY_CAP	1000

static const char* const sortCategoryByItemType[][2] =
{
	// weapons
	{"abow", "Amazon Bows"}, {"ajav", "Amazon Javelins"}, {"aspe", "Amazon Spears"}, {"axe", "Axes"},
	{"bow", "Bows"}, {"club", "Clubs"}, {"h2h", "Katars"}, {"h2h2", "Katars"}, {"hamm", "Hammers"},
	{"jave", "Javelins"}, {"knif", "Daggers"}, {"mace", "Maces"}, {"orb", "Orbs"}, {"pole", "Polearms"},
	{"scep", "Scepters"}, {"spea", "Spears"}, {"staf", "Staves"}, {"swor", "Swords"},
	{"taxe", "Throwing Axes"}, {"tkni", "Throwing Knives"}, {"tpot", "Throwing Potions"},
	{"wand", "Wands"}, {"xbow", "Crossbows"},
	// armors
	{"ashd", "Paladin Shields"}, {"belt", "Belts"}, {"boot", "Boots"}, {"circ", "Circlets"},
	{"glov", "Gloves"}, {"grim", "Warlock Grimoires"}, {"head", "Necromancer Heads"}, {"helm", "Helms"},
	{"pelt", "Druid Pelts"}, {"phlm", "Barbarian Helms"}, {"shie", "Shields"}, {"tors", "Body Armor"},
};

static const char* const sortMiscSubByItemType[][2] =
{
	{"amul", "Jewelry"}, {"ring", "Jewelry"}, {"jewl", "Jewels"}, {"cjwl", "Jewels"},
	{"scha", "Charms"}, {"mcha", "Charms"}, {"lcha", "Charms"}, {"csch", "Charms"}, {"torc", "Charms"},
	{"gema", "Gems"}, {"gemd", "Gems"}, {"geme", "Gems"}, {"gemr", "Gems"}, {"gems", "Gems"}, {"gemt", "Gems"}, {"gemz", "Gems"},
	{"rune", "Runes"},
	{"hpot", "Potions"}, {"mpot", "Potions"}, {"rpot", "Potions"}, {"spot", "Potions"}, {"apot", "Potions"},
	{"wpot", "Potions"}, {"elix", "Potions"},
	{"book", "Tomes"}, {"scro", "Scrolls"}, {"key", "Keys"}, {"bowq", "Quivers"}, {"xboq", "Quivers"},
	{"ques", "Quest Items"}, {"play", "Quest Items"}, {"body", "Quest Items"}, {"herb", "Quest Items"},
};

static const char* const sortMiscSubOrder[] =
{
	"Jewelry", "Jewels", "Charms", "Gems", "Runes", "Potions", "Tomes", "Scrolls", "Keys", "Quivers", "Quest Items",
};

#define SORT_COUNT(T) (sizeof(T) / sizeof(T[0]))

static const char* sortLookup(const char* const table[][2], int nb, const char* key, const char* notFound)
{
	for (int i = 0; i < nb; i++)
		if (!strcmp(table[i][0], key))
			return table[i][1];
	return notFound;
}

static int sortQualityOf(const SortItem* item)
{
	if (item->isRuneword) return SORT_QUALITY_RUNEWORD;
	int q = item->quality ? item->quality : 2;
	if (q == 7) return SORT_QUALITY_UNIQUE;
	if (q == 5) return SORT_QUALITY_SET;
	if (q == 8) return SORT_QUALITY_CRAFTED;
	if (q == 6 || q == 9) return SORT_QUALITY_RARE;
	if (q == 4) return SORT_QUALITY_MAGIC;
	return SORT_QUALITY_NORMAL;
}

static int sortSubQualityOf(const SortItem* item)
{
	return item->quality ? item->quality : 2;
}

static bool sortIsMisc(const SortItem* item)
{
	return !strcmp(item->category, "Misc");
}

static int sortClassOrderOf(const SortItem* item)
{
	if (item->sortQuality == SORT_QUALITY_NORMAL)
	{
		if (sortIsMisc(item)) return 0;
		if (item->isWeapon) return 1;
		if (item->isArmor) return 2;
		return 0;
	}
	if (sortIsMisc(item)) return 2;
	if (item->isWeapon) return 0;
	if (item->isArmor) return 1;
	return 2;
}

static int sortMiscSubOrderOf(const SortItem* item)
{
	for (int i = 0; i < (int)SORT_COUNT(sortMiscSubOrder); i++)
		if (!strcmp(sortMiscSubOrder[i], item->miscSub))
			return i;
	return 99;
}

static int sortIndexOf(const char* list, char c)
{
	const char* p = c ? strchr(list, c) : NULL;
	return p ? (int)(p - list) : -1;
}

// Runes r01..r33, gems by color then grade, charms small/large/grand, quest items.
static int sortInnerOrderOf(const SortItem* item)
{
	const char* code = item->code;
	if (!sortIsMisc(item)) return 0;
	if (!strcmp(item->miscSub, "Runes"))
	{
		if (code[0] == 'r' && isdigit((unsigned char)code[1]) && isdigit((unsigned char)code[2]) && !code[3])
			return (code[1] - '0') * 10 + (code[2] - '0');
		return 999;
	}
	if (!strcmp(item->miscSub, "Gems"))
	{
		char c2 = code[0] && code[1] ? code[2] : 0;
		if (code[0] == 's' && code[1] == 'k')
		{
			int t = sortIndexOf("cfulz", c2);
			return 6 * 10 + (t < 0 ? 9 : t);
		}
		int tier = 9;
		if (code[0] == 'g')
			switch (code[1])
			{
			case 'c': tier = 0; break;
			case 'f': tier = 1; break;
			case 's': tier = 2; break;
			case 'l': case 'z': tier = 3; break;
			case 'p': tier = 4; break;
			}
		int color = sortIndexOf("vybgrw", c2);
		return (color < 0 ? 9 : color) * 10 + tier;
	}
	if (!strcmp(item->miscSub, "Charms"))
	{
		if (!strcmp(code, "cm1")) return 0;
		if (!strcmp(code, "cm2")) return 1;
		if (!strcmp(code, "cm3")) return 2;
		if (!strcmp(code, "cs2")) return 3;
		return 9;
	}
	if (!strcmp(item->miscSub, "Quest Items"))
	{
		static const char* const questOrder[][2] = {{"tes","0"},{"ceh","1"},{"bet","2"},{"fed","3"},{"pk1","10"},{"pk2","11"},{"pk3","12"}};
		for (int i = 0; i < (int)SORT_COUNT(questOrder); i++)
			if (!strcmp(code, questOrder[i][0]))
				return atoi(questOrder[i][1]);
		return 20;
	}
	return 0;
}

static void sortStashPrepare(SortItem* item)
{
	item->sortQuality = sortQualityOf(item);
	item->category = sortLookup(sortCategoryByItemType, SORT_COUNT(sortCategoryByItemType), item->itype, "Misc");
	item->miscSub = sortLookup(sortMiscSubByItemType, SORT_COUNT(sortMiscSubByItemType), item->itype, "");
	item->classOrder = sortClassOrderOf(item);
	item->miscSubOrder = sortMiscSubOrderOf(item);
	item->innerOrder = sortInnerOrderOf(item);
	item->groupItem[0] = 0;
	if (sortIsMisc(item))
	{
		strcpy(item->groupItem, item->itype);
		strcat(item->groupItem, "|");
		strcat(item->groupItem, item->code);
	}
}

// strcmp of the lower case strings (toLowerCase() then < in sort_stash.js).
static int sortStrCmpLower(const char* a, const char* b)
{
	while (*a && tolower((unsigned char)*a) == tolower((unsigned char)*b)) { a++; b++; }
	return tolower((unsigned char)*a) - tolower((unsigned char)*b);
}

#define SORT_CMP_INT(A, B)	if ((A) != (B)) return (A) < (B) ? -1 : 1
#define SORT_CMP_STR(A, B)	{ int r = sortStrCmpLower(A, B); if (r) return r < 0 ? -1 : 1; }

// compareSortableItems / buildSortKey
static int sortCompareItems(const SortItem* a, const SortItem* b)
{
	SORT_CMP_INT(a->sortQuality, b->sortQuality);
	if (a->sortQuality == SORT_QUALITY_RUNEWORD)
		SORT_CMP_INT(a->runeword, b->runeword);
	if (a->sortQuality == SORT_QUALITY_SET)
	{
		SORT_CMP_INT(a->setKey, b->setKey);
		SORT_CMP_INT(a->bodyPos, b->bodyPos);
	}
	else
	{
		bool aMisc = sortIsMisc(a), bMisc = sortIsMisc(b);
		SORT_CMP_INT(a->classOrder, b->classOrder);
		SORT_CMP_INT(aMisc ? 0 : a->specialRank, bMisc ? 0 : b->specialRank);
		SORT_CMP_INT(aMisc ? 0 : a->bodyPos, bMisc ? 0 : b->bodyPos);
		SORT_CMP_STR(a->category, b->category);
		SORT_CMP_INT(a->miscSubOrder, b->miscSubOrder);
		SORT_CMP_INT(a->isEthereal, b->isEthereal);
		SORT_CMP_INT(sortSubQualityOf(a), sortSubQualityOf(b));
		SORT_CMP_INT(a->tier, b->tier);
		SORT_CMP_INT(a->innerOrder, b->innerOrder);
		SORT_CMP_STR(a->groupItem, b->groupItem);
		SORT_CMP_INT(a->picture, b->picture);
	}
	SORT_CMP_STR(a->code, b->code);
	SORT_CMP_INT(a->row, b->row);
	SORT_CMP_INT(a->unitId, b->unitId);
	return 0;
}

// _sortGroupKeyOf : a new group starts on a new page.
static bool sortSameGroup(const SortItem* a, const SortItem* b)
{
	if (a->sortQuality != b->sortQuality) return false;
	if (a->sortQuality == SORT_QUALITY_SET) return a->setKey == b->setKey;
	if (a->sortQuality == SORT_QUALITY_RUNEWORD) return a->runeword == b->runeword;
	bool aMisc = sortIsMisc(a);
	if (aMisc != sortIsMisc(b)) return false;
	if (!aMisc)
		return a->classOrder == b->classOrder && !sortStrCmpLower(a->category, b->category);
	if (strcmp(a->miscSub, b->miscSub)) return false;
	if (!strcmp(a->miscSub, "Charms"))
	{
		static const char* const sizes[] = {"cm1", "cm2", "tch", "cm3", "cs2"};
		static const int sizeKey[] = {1, 2, 2, 3, 3};
		int sa = 9, sb = 9;
		for (int i = 0; i < (int)SORT_COUNT(sizes); i++)
		{
			if (!strcmp(a->code, sizes[i])) sa = sizeKey[i];
			if (!strcmp(b->code, sizes[i])) sb = sizeKey[i];
		}
		return sa == sb;
	}
	if (!strcmp(a->miscSub, "Quest Items"))
	{
		static const char* const quests[] = {"tes", "ceh", "bet", "fed", "pk1", "pk2", "pk3"};
		static const int questKey[] = {1, 1, 1, 1, 2, 2, 2};
		int qa = 3, qb = 3;
		for (int i = 0; i < (int)SORT_COUNT(quests); i++)
		{
			if (!strcmp(a->code, quests[i])) qa = questKey[i];
			if (!strcmp(b->code, quests[i])) qb = questKey[i];
		}
		return qa == qb;
	}
	return true;
}

// Row key of packSortedItems : same code, ethereal and quality on the same row.
static bool sortSameRow(const SortItem* a, const SortItem* b)
{
	return !strcmp(a->code, b->code) && a->isEthereal == b->isEthereal && sortSubQualityOf(a) == sortSubQualityOf(b);
}

struct SortPages
{
	unsigned char* cells;
	int nbPages;
	int gridW;
	int gridH;
};

static bool sortEnsurePage(SortPages* pages, int page)
{
	if (page < pages->nbPages) return true;
	int size = pages->gridW * pages->gridH;
	unsigned char* cells = (unsigned char*)realloc(pages->cells, (page + 1) * size);
	if (!cells) return false;
	memset(cells + pages->nbPages * size, 0, (page + 1 - pages->nbPages) * size);
	pages->cells = cells;
	pages->nbPages = page + 1;
	return true;
}

static bool sortFits(SortPages* pages, int page, int x, int y, int w, int h)
{
	unsigned char* occ = pages->cells + page * pages->gridW * pages->gridH;
	for (int dy = 0; dy < h; dy++)
		for (int dx = 0; dx < w; dx++)
			if (occ[(y + dy) * pages->gridW + x + dx]) return false;
	return true;
}

static void sortOccupy(SortPages* pages, int page, int x, int y, int w, int h)
{
	unsigned char* occ = pages->cells + page * pages->gridW * pages->gridH;
	for (int dy = 0; dy < h; dy++)
		for (int dx = 0; dx < w; dx++)
			occ[(y + dy) * pages->gridW + x + dx] = 1;
}

// packSortedItems. Return the number of pages used, or -1 if an item can't be placed.
static int sortPack(SortItem** sorted, int nb, int gridW, int gridH, bool groupBreak, bool rowBreak, SortPlace* places)
{
	SortPages pages = {NULL, 0, gridW, gridH};
	int curPage = 0, rowTop = 0, rowCol = 0, rowH = 0, used = 0;
	SortItem* prev = NULL;
	for (int i = 0; i < nb; i++)
	{
		SortItem* item = sorted[i];
		int w = item->width ? item->width : 1;
		int h = item->height ? item->height : 1;
		if (w > gridW || h > gridH) { free(pages.cells); return -1; }
		if (groupBreak && prev && !sortSameGroup(item, prev))
		{
			curPage++;
			rowTop = 0; rowCol = 0; rowH = 0;
		}
		int page = -1, x = 0, y = 0;
		if (rowBreak)
		{
			if (rowCol > 0 && (!sortSameRow(item, prev) || rowCol + w > gridW))
			{
				rowTop += rowH; rowCol = 0; rowH = 0;
			}
			if (rowTop + h > gridH)
			{
				curPage++; rowTop = 0; rowCol = 0; rowH = 0;
			}
			if (curPage < SORT_PAGE_SAFETY_CAP && sortEnsurePage(&pages, curPage))
			{
				page = curPage; x = rowCol; y = rowTop;
				rowCol += w;
				if (h > rowH) rowH = h;
			}
		}
		else
		{
			for (int tryPage = curPage; page < 0 && tryPage < SORT_PAGE_SAFETY_CAP; tryPage++)
			{
				if (!sortEnsurePage(&pages, tryPage)) break;
				for (int r = 0; page < 0 && r <= gridH - h; r++)
					for (int c = 0; page < 0 && c <= gridW - w; c++)
						if (sortFits(&pages, tryPage, c, r, w, h))
						{
							page = tryPage; x = c; y = r;
						}
			}
			if (page >= 0) curPage = page;
		}
		if (page < 0) { free(pages.cells); return -1; }
		sortOccupy(&pages, page, x, y, w, h);
		places[i].page = page;
		places[i].x = x;
		places[i].y = y;
		if (page + 1 > used) used = page + 1;
		prev = item;
	}
	free(pages.cells);
	return used;
}

static int sortCompareForQsort(const void* a, const void* b)
{
	return sortCompareItems(*(const SortItem* const*)a, *(const SortItem* const*)b);
}

/*
Sort the items and place them. places[i] is the place of items[i].
If the pages needed are more than maxPages (0 : no limit), the items are packed again without the
group and row breaks (like planSharedStashSort). Return the number of pages used, or -1 if it doesn't fit.
*/
static int sortStashPlan(SortItem* items, int nb, int gridW, int gridH, unsigned int maxPages, SortPlace* places, bool* compact)
{
	*compact = false;
	if (nb <= 0) return 0;
	SortItem** sorted = (SortItem**)malloc(nb * sizeof(SortItem*));
	SortPlace* sortedPlaces = (SortPlace*)malloc(nb * sizeof(SortPlace));
	if (!sorted || !sortedPlaces) { free(sorted); free(sortedPlaces); return -1; }
	for (int i = 0; i < nb; i++)
	{
		sortStashPrepare(&items[i]);
		sorted[i] = &items[i];
	}
	qsort(sorted, nb, sizeof(SortItem*), sortCompareForQsort);

	int used = sortPack(sorted, nb, gridW, gridH, true, true, sortedPlaces);
	if (used < 0 || (maxPages && (unsigned int)used > maxPages))
	{
		*compact = true;
		used = sortPack(sorted, nb, gridW, gridH, false, false, sortedPlaces);
		if (maxPages && (unsigned int)used > maxPages) used = -1;
	}
	if (used >= 0)
		for (int i = 0; i < nb; i++)
			places[sorted[i] - items] = sortedPlaces[i];
	free(sorted);
	free(sortedPlaces);
	return used;
}

/*================================= END OF FILE =================================*/
