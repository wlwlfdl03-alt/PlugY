/*
Test driver of PlugY/sortStashCore.h on Linux (no game needed) :

  g++ -O1 -o sort_stash_test tools/sort_stash_test.cpp
  ./sort_stash_test [gridW gridH maxPages] < items.txt

Input : one item per line
  unitId code itype width height quality isRuneword ethereal weapon armor bodyPos specialRank tier setKey row runeword picture
  [baseCode itemTier] : base of the unique/set row and tier of code (upgraded items), default code and tier
Output : "pages N compact C" then one line per item, in input order : "unitId page x y".
*/
#include <stdio.h>
#include "../PlugY/sortStashCore.h"

int main(int argc, char** argv)
{
	int gridW = argc > 2 ? atoi(argv[1]) : 10;
	int gridH = argc > 2 ? atoi(argv[2]) : 10;
	unsigned int maxPages = argc > 3 ? (unsigned int)atoi(argv[3]) : 0;

	int nb = 0, size = 64;
	SortItem* items = (SortItem*)malloc(size * sizeof(SortItem));
	SortItem it;
	int rw, eth, weap, arm, nbRead;
	char line[256];
	memset(&it, 0, sizeof(it));
	while (fgets(line, sizeof(line), stdin) && (nbRead = sscanf(line, "%u %4s %4s %d %d %d %d %d %d %d %d %d %d %d %d %d %d %4s %d",
			&it.unitId, it.code, it.itype, &it.width, &it.height, &it.quality, &rw, &eth, &weap, &arm, &it.bodyPos,
			&it.specialRank, &it.tier, &it.setKey, &it.row, &it.runeword, &it.picture, it.baseCode, &it.itemTier)) >= 17)
	{
		if (nbRead < 19)
		{
			strcpy(it.baseCode, it.code);
			it.itemTier = it.tier;
		}
		it.isRuneword = rw != 0;
		it.isEthereal = eth != 0;
		it.isWeapon = weap != 0;
		it.isArmor = arm != 0;
		if (nb == size)
			items = (SortItem*)realloc(items, (size *= 2) * sizeof(SortItem));
		items[nb++] = it;
		memset(&it, 0, sizeof(it));
	}

	SortPlace* places = (SortPlace*)malloc((nb ? nb : 1) * sizeof(SortPlace));
	bool compact;
	int pages = sortStashPlan(items, nb, gridW, gridH, maxPages, -1, places, &compact);
	printf("pages %d compact %d\n", pages, compact ? 1 : 0);
	if (pages >= 0)
		for (int i = 0; i < nb; i++)
			printf("%u %d %d %d\n", items[i].unitId, places[i].page, places[i].x, places[i].y);
	free(items);
	free(places);
	return 0;
}
