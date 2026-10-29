# PlugY

PlugY, The Survival Kit - Plug-in for Diablo II.

Original PlugY by Yohann NICOLAS ([ChaosMarc/PlugY](https://github.com/ChaosMarc/PlugY)). This fork adds support for **Diablo II Classic** characters. PlugY features that worked only with Lord of Destruction now work with Classic characters too.

## Supported versions

| Game | Versions |
|---|---|
| Diablo II: Lord of Destruction | 1.09b - 1.13d, 1.14d |
| Diablo II Classic | 1.09b, 1.09d, 1.13c, 1.13d, 1.14d |

## Classic support

- Multipage personal stash and shared stash.
- Separate shared stash files for Classic characters (`_CLASSIC_SharedStashSave.sss`, `_CLASSIC_HC_SharedStashSave.sss`).
- Stats and skills unassignment, and the extra stats pages.
- Big stash (`ActiveBigStash=1`): Classic games use the same 10x10 stash screen as LoD.

## Stash sort

- A sort button on the big stash screen (`ActiveMultiPageStash=1`, `ActiveBigStash=1`, 1.09b, 1.09d, 1.13c, 1.13d, 1.14d) sorts the items of all the pages of the stash shown (personal or shared), in LoD and Classic games.
- At 640x480 the button is behind the life orb but can still be clicked. `PosXSortBtn` and `PosYSortBtn` in the `[STASH]` section of `PlugY.ini` move it.
