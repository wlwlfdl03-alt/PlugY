# PlugY (Diablo II Classic 지원 포크)

원본 PlugY(Yohann NICOLAS, ChaosMarc/PlugY)의 포크다. LoD 전용이던 PlugY 기능을 Diablo II Classic 캐릭터에서도 쓸 수 있게 하는 것이 목표다.

- LoD: 1.09b - 1.13d, 1.14d (원본과 같음)
- Classic: 1.09b, 1.09d, 1.13c, 1.13d, 1.14d (인게임 테스트 완료)

## 작업 규칙 (사용자가 정한 것)

- 파일은 수정하기 전에 무엇을 어떻게 바꿀지 사용자에게 보고하고, 허가를 받은 뒤 고친다. 코드, 스크립트, 문서 모두 같다.
- 문서(`CLAUDE.md`, `README.md`)는 모든 작업이 끝난 뒤 마지막에 갱신한다.
- 인게임 동작이나 게임 지식에 확신이 없으면 추측하지 말고 반드시 사용자에게 묻는다.
- 원본 소스의 주석은 수정하지 않는다.
- 게임 바이너리(`D2Client.dll`, `Game.exe` 등)는 저작권 파일이므로 저장소에 커밋하지 않는다. 분석이 필요하면 사용자에게 업로드를 요청하고, 세션 임시 폴더에서만 쓴다.
- `README.md`는 영어로 짧게 유지한다. 문서를 바꿀 때 사용자가 예문을 먼저 보여 달라고 하면 파일을 고치기 전에 보여 준다.
- 변경은 작업 브랜치에서 커밋하고 PR로 `master`에 병합한다. PR은 사용자가 요청할 때만 만든다.
- 병합된 PR의 브랜치는 재사용하지 않는다. 같은 이름의 브랜치를 최신 `master`에서 다시 시작한다.

## 빌드

- 원래 프로젝트는 VS2008 `PlugY/PlugY.vcproj`라서 지금의 MSBuild로는 빌드할 수 없다. `.gitignore`가 `*.vcxproj`를 무시한다(작성자가 로컬에서 변환해 쓰는 파일).
- CI 전용 프로젝트 `.github/build/PlugY.vcxproj`가 있다. 원본과 같은 소스와 Release|Win32 설정이고, 다른 점은 둘이다.
  - VS2022(v143) 툴셋
  - 정적 CRT(`/MT`)
- `.github/workflows/build.yml`가 push, PR, 수동 실행 때 windows-2022에서 빌드하고 `PlugY` 아티팩트를 올린다. 사용자는 이 아티팩트로 테스트한다.
  - 아티팩트는 게임 폴더 구조와 같다. 최상위에 `PlugY.dll`, `PlugY.pdb`가 있고, `PlugY\`에 `PlugYInstaller/PlugY/`의 데이터 파일(dc6, `LocalizedStrings.ini` 등)이 있다.
  - 새 이미지나 문자열을 추가하면 DLL만으로는 동작하지 않는다. `PlugY\`의 파일도 함께 복사해야 한다(없는 dc6는 "Don't find Buttons Images File" 오류로 종료된다).
- `PlugY/`에 .cpp를 추가하면 `.github/build/PlugY.vcxproj`의 `ClCompile` 목록에도 추가한다.
- Linux에서는 `tools/check_syntax.sh`로 clang 문법 검사만 할 수 있다. 필요한 패키지는 `clang`과 `mingw-w64-i686-dev`이다.
  - 이 검사는 링크를 하지 않는다. 함수 시그니처를 바꾸면 헤더 선언과 모든 호출부를 직접 grep으로 확인한다(PR #2의 LNK2001이 이렇게 생겼다).
  - `D2functions.cpp`, `LocalizedStrings.cpp`, `WorldEvent.cpp`는 MSVC 전용 문법이라 원래 clang에서 오류가 난다. 오류가 새로 생겼는지만 본다.

## 코드 관례

- 소스는 대부분 CRLF이다. `README.md`, `.gitignore`, 워크플로는 LF이고, 마지막 줄바꿈이 없는 파일도 있다.
  - 스크립트로 파일을 고친 뒤에는 `tools/fix_eol.py`로 HEAD의 줄바꿈 형식을 되돌린다.
  - Python에서는 `open(..., newline='')`로 읽고 쓴다.
- `PlugYInstaller/*.txt`는 latin-1(Windows-1252)이다.
- 버전별 주소는 `R8(DLL, 109b, 109d, 110, 111, 111b, 112, 113c, 113d, 114d)` 매크로를 쓴다. 값은 DLL 기준 RVA다.
  - 1.14d는 모든 DLL이 `Game.exe` 하나에 합쳐져 있다. 그래서 `offset_D2Client`가 Game.exe 베이스(0x400000)다.
- 메모리 패치 함수는 기존 값을 확인한 뒤에 쓴다(`PlugY/ModifMemory.cpp`).
  - 대상 함수: `memt_byte`, `memt_dword`(`MEMT_DWORD`), `memt_ref4`(`MEMT_REF4`), `memc_ref4`(`MEMC_REF4`)
  - `ActiveCheckMemory=1`이면 값이 다를 때 "BYTE/DWORD ... wanted but ... found" 오류창을 띄우고 종료한다.
  - `MEMC_REF4`/`MEMT_REF4`의 `mem_seek`는 명령어 시작이 아니라 4바이트 피연산자를 가리켜야 한다. `E8 xx xx xx xx`라면 +1이다.
  - `memt_byte`로 opcode를 바꾼 뒤 이어서 `MEMT_REF4`를 쓰면 위치가 자동으로 넘어간다.
  - PR #3이 이 실수(`8CC05` → `8CC06`)를 고친 것이다.
- 재배치되는 절대 주소(예: IsLodGame `ptIsLodGame`)를 기존 값으로 확인할 때는 상수 대신 `pt...` 변수로 계산한다.

## Classic 지원 구조

### 저장/로드 (버전 무관)
- 원본은 `isLODGame`/`D2isLODGame()`으로 클래식에서 PlugY 저장 데이터를 막았다. 이 체크를 없앴다(`LoadPlayerData.cpp`, `SavePlayerData.cpp`, `PlayerCustomData.cpp`).
- 공유 창고 파일은 클래식과 LoD를 분리한다.
  - LoD: `_LOD_` / `_LOD_HC_`
  - 클래식: `_CLASSIC_` / `_CLASSIC_HC_`
  - 이름은 `getSharedSaveFilePrefix(isHardcore, isLOD)`(`SharedSaveFile.cpp`)가 정한다.
- 싱글은 `isLODGame(PCGame)`(`SharedSaveFile.cpp`)으로 클래식/LoD를 판별한다.
  - Game+0x70(`isLODGame`)을 읽는다. 1.09b만 Game+0x50이다. 1.09d는 +0x70이다.
  - 1.09b D2Game은 난이도를 +0x4D, 확장팩 여부를 +0x50에서 읽는다(1.09d는 +0x6D, +0x70). Unit+0xA4(Game 포인터)를 읽은 직후의 접근을 1.09b와 1.09d에서 비교해서 확인했다.
  - 처음에는 +0x70을 그대로 읽어서, 1.09b 클래식 캐릭터가 `_LOD_` 공유 창고를 썼다(PR #13에서 수정).
- TCP/IP 클라이언트는 `.d2s` 상태 바이트(오프셋 0x24)를 읽는다. 0x04는 하드코어, 0x20은 확장팩이다(`isExpansionCharacter`).
  - MP 저장(`SaveMPPlayerCustomData`)도 받은 d2s의 같은 바이트를 쓴다.
- `backupSaveFiles(name, isHardCoreGame, isLOD)`는 해당하는 공유 창고 파일을 백업한다. `/renamechar`(`Commands.cpp`)는 `D2isLODGame()`을 넘긴다.

### UI (버전 무관)
- 창고, 스탯, 스킬, 추가 스탯 페이지의 `!D2isLODGame()` 체크를 없앴다.
- 룬워드 페이지(`NewInterface_Runewords.cpp`, `/page 6`)는 LoD 전용으로 남겼다.
- BigStash를 끄면 클래식은 원래 6x4 창고 화면을 쓴다.
  - 그 위에 PlugY 버튼을 그리는 기본 Y 위치가 다르다(`Interface_Stash.cpp`). 골드 버튼 0xE4, 페이지 이름 0xD6, 골드 필드 0xF2이고, LoD는 각각 0x1A8, 0x19A, 0x1B6이다.
  - 이 값은 D2Client의 클래식 분기 상수(골드 필드 -0xF2/-0xF4, "Gold Max" -0xDB)에서 왔고, 1.13c, 1.13d, 1.14d 모두 같다.
  - 판별은 `isLodStashLayout()`(`BigStash.cpp`)이 한다.

### BigStash 클래식 화면 (`BigStash.cpp`, 1.09b / 1.09d / 1.13c / 1.13d / 1.14d만)
- inventory.txt 행(컴파일 시 "Expansion" 구분 행은 빠진다):
  - "Bank Page 1"(클래식 창고) = 8, 800x600 = 24
  - "Big Bank Page 1"(LoD) = 12, 800x600 = 28
- 클래식 행은 패치가 설치된 버전에서만 10x10으로 바꾼다(`active_classicBigStash`).
- 클래식 창고 배경은 `%s\ui\panel\bank`이다. LoD 배경 `%s\ui\panel\TradeStash`는 Fog #10227(LoD 설치 여부)이 참일 때만 불러온다. PlugY는 이 TradeStash 경로만 `PlugY\TradeStash`로 바꾼다.
- 창고 화면의 레이아웃 분기(배경, "Gold Max" 위치, 골드 필드 그리기, 골드 필드 클릭 판정)가 IsLodGame 대신 `stashLodLayout`(= `isLodStashLayout()`)을 보게 한다.
  - 1.13c, 1.13d: IsLodGame을 읽는 명령의 주소 피연산자를 `&stashLodLayout`으로 바꾼다. 값은 창고를 그리기 직전(`caller_updateStashLayout`)과 클릭 판정 시작(`caller_isOnStashGoldField`)에 갱신한다.
  - 1.09b, 1.09d, 1.14d: 창고 코드가 IsLodGame 변수를 직접 읽지 않고 D2isLODGame 함수를 호출한다(1.09b `6FAABA00`, 1.09d `6FAAB9F0`, 1.14d `Game.0044DCC0`). 이 호출을 `caller_isLodStashLayout`으로 바꾼다.
    - 1.14d는 4곳이다. 1.09는 골드 필드를 팝업과 마우스 처리 함수에서 다시 판정해서 7곳이다(누름 2곳, 뗌 1곳).
  - 다섯 버전 모두 Fog #10227 호출을 `caller_loadTradeStash`로 바꿔서, LoD가 없어도 PlugY 배경을 불러온다. 이 호출 대상은 1.09에서 D2Client 안의 jmp thunk이고, 1.14d에서 Game.exe 내부 함수 `00408F20`이다.
- 격자와 패널 범위는 레이아웃 분기가 아니라 inventory.txt 행(8, 12)에서 읽으므로 건드리지 않는다. 1.09에서도 같다(1.09b `6FADA97A` ~ `6FADA9B2`).
- 닫기 버튼 위치 함수(1.13c `6FB3BEB0`/`6FB3BEE0`)는 다른 패널과 함께 쓰는 코드라 건드리지 않았다. 클래식은 LoD보다 X로 3px, Y로 1px 다르다.
  - 1.09는 닫기 버튼 위치를 창고 함수 안에서 직접 계산한다(X 0x113/0x110, Y -0x41/-0x40). 다른 버전과 같게 그대로 두었다.
- 1.09d D2Client의 창고 코드는 1.09b와 주소가 같다. 명령어 단위로 비교했고, 골드 필드 그리기 함수의 스택 변수 순서만 다르다.

| 패치 | 1.09b | 1.09d | 1.13c | 1.13d | 1.14d |
|---|---|---|---|---|---|
| Fog #10227 호출 피연산자 | 45B06 (→C093A) | 45B06 (→BFCBA) | 8CC06 (→BF8C) | 943E6 (→BEA0) | 89E9D (→8F20) |
| 기존 배경 경로 패치 `PUSH 104` | 45B1C | 45B1C | 8CC1C | 943FC | 89EB5 |
| bank 이미지 변수 | 120D94 (참고) | 11FBF4 (참고) | 11BC74 | 11CB64 | 3BCC2C (참고) |
| 레이아웃 갱신 `MOV EAX,[bank]` | - | - | 998D5 | 9DCA5 | - |
| 배경 선택 | 38ECF (call) | 38ECF (call) | 998E4 | 9DCB4 | 8F19C (call) |
| "Gold Max" 위치 | 38FF1 (call) | 38FF1 (call) | 999FB | 9DDCB | 8F2B1 (call) |
| 골드 필드 그리기 | 42D46 (call) | 42D46 (call) | 8D14C | 9119C | 8820D (call) |
| 골드 필드 팝업 | 42D9E (call) | 42D9E (call) | - | - | - |
| 골드 필드 클릭 판정 | 45052, 45341 누름, 4577E 뗌 (call) | 1.09b와 같음 | 8B6B0 (함수 시작) | 8FA30 | 89921 (call) |
| D2isLODGame 함수 (call 대상) | BA00 | B9F0 | - | - | 4DCC0 |
| IsLodGame 변수 | 111BC4 | 110A24 | 119854 | 1087B4 | 3A04F4 |

## 창고 정렬 버튼 (`SortStash.cpp`, `sortStashCore.h`)

- 사용자가 준 웹앱 정렬 로직 `sort_stash.js`의 `sortPlugyAllPages`를 옮긴 것이다. 원본 JS는 저장소에 없다.
  - 정렬은 `compareSortableItems`, 배치는 `packSortedItems`(groupBreak, rowBreak 켬)이다.
  - 오름차순이다. 노말부터 유니크 순으로 첫 페이지부터 채운다.
- 사용자가 정한 차이:
  - 룬워드는 품질 다음에 룬워드(runes.txt 행 순서)로 묶는다. 룬워드마다 새 페이지이고, 그 안에서는 원래 키(무기/방어구, 부위, 종류 등)로 정렬한다.
    - 룬워드는 소켓의 룬을 runes.txt 행과 맞춰서 찾는다(`getRunewordRow`). D2Common #10822 `ITEMS_GetRunesTxtRecordFromItem`(D2MOO)과 같은 규칙이다. 순서대로 같은 룬이고, Etypes에 없고 Itypes에 있어야 한다.
  - 12차 키(그림 변형)는 `ItemData+0x49`(`varGfx`, D2MOO `nInvGfxIdx`)이다. 같은 그림끼리 붙이기만 하고, 줄은 나누지 않는다(JS와 같음).
  - 13차 키(표시 이름)는 아이템 코드, 그다음 유니크/세트 행 번호로 비교한다. 게임 언어와 무관하다.
  - 세트 순서는 이름 대신 sets.txt 행 순서이다.
  - 정렬이 끝나면 그 창고 모든 페이지의 이름과 인덱스 플래그를 지운다.
  - 채팅 명령어는 없다. 창고 버튼만 있다.
  - `MaxPersonnalPages`/`MaxSharedPages`로 페이지가 모자라면 JS 공유창고 경로처럼 그룹/줄 구분 없이 다시 배치한다. 그래도 모자라면 아무것도 옮기지 않는다.
    - 빈칸 없는 배치도 정렬 순서대로 넣고 앞 페이지로 되돌아가 채우지 않는다. 그래서 전체 칸 수가 남아도 실패할 수 있다.
- 버튼 조건: `ActiveMultiPageStash=1`, `ActiveBigStash=1`, 1.09b/1.09d/1.13c/1.13d/1.14d이다(`active_sortStash` = `active_bigStash && active_classicBigStash`).
  - 버튼 그림은 `PlugY\SortBtns.dc6`이다. 사용자가 고른 시안 9번(금색 순환 화살표)이다.
  - 그림은 `tools/make_sort_btn.py <act1 pal.dat>`로 만든다. 팔레트는 저작권 파일이라 커밋하지 않는다.
  - 기본 위치는 아래 버튼 줄(Y=0x40)이다. X는 800x600에서 0xE0(다음 인덱스 버튼 오른쪽), 640x480에서 0x4F(공유 버튼 왼쪽)이다.
  - 640x480에서는 버튼이 HP 구체 뒤에 가려진다. 클릭은 된다.
    - 창고 패널에 겹치지 않는 32x32 자리가 없다. 아래 줄은 공유 ~ 닫기 버튼으로 차 있고, 위쪽 줄의 틈은 약 14px(골드 버튼과 필드 사이), 골드 넣기 버튼 아래는 약 22px이다.
    - 사용자가 이 위치를 그대로 두기로 했다. `PlugY.ini` `[STASH]`의 `PosXSortBtn`/`PosYSortBtn`으로 옮길 수 있다(두 해상도에 함께 적용된다).
- 동작 흐름:
  - 클라이언트 버튼이 `US_SORT_STASH`를 보내면 서버가 `sortStash`를 실행한다. 서버가 정렬하고 아이템을 옮긴다.
  - 서버는 결과의 체크섬만 담은 메시지 하나(`UC_SORT_STASH`: 공유/압축 플래그, 아이템 수, 체크섬)를 보낸다.
  - 클라이언트는 자기 아이템으로 같은 정렬을 직접 계산한다(`planStashSort`, `sortStashClient`). 아이템 수와 체크섬이 서버와 같을 때만 옮긴다(`moveStashItems`). 다르면 아무것도 옮기지 않고 `PlugY.log`에 남긴다.
  - 결과는 아이템 데이터와 unitId에만 달려 있고 입력 순서와는 무관하다. 동률은 unitId로 정한다.
    - 그래서 클라이언트에는 서버와 같은 압축 여부만 알려 주면 된다. `MaxPages` 설정이 달라도 상관없다.
  - 처음에는 아이템마다 `UC_SORT_ITEM`을 보냈다. 이 방식은 TCP/IP에서 실패했다.
    - 1만 개(585페이지) 창고에서 클라이언트가 10941개 중 7266개만 받았다.
    - 마지막 메시지를 받지 못했고, PlugY 화면의 버튼과 아이템이 눌리지 않았다.
  - 아이템의 unitId(GUID)는 `Unit+0x0C`이다. PlugY 구조체에서는 이름이 `nItemNum`이다(`UNIT_GUID`). `Unit::nUnitId`(+0x08)는 메모리 풀 포인터라서 싱글에서 0이다.
    - 1.09만 GUID가 `Unit+0x08`이다(D2Client 유닛 해시 검색이 `[unit+0x8]`과 ID를 비교한다).
    - 처음에 `nUnitId`를 써서 모든 아이템이 ID 0으로 전송됐다. 그 결과 클라이언트가 같은 아이템을 세 번 옮겼고, 아이템이 사라지고 페이지를 넘길 때 게임이 멈췄다.
- D2Common 동작은 D2MOO(https://github.com/ThePhrozenKeep/D2MOO, 1.10f)의 `source/D2Common/src/D2Inventory.cpp`로 확인한다.
  - `D2InvAddItem`(#10249)은 `INVENTORY_PlaceItemInGrid`이다. 칸이 비어 있지 않으면 목록에 연결하기 전에 FALSE를 돌려준다. 이때 그 아이템은 어디에도 속하지 않게 된다.
  - 이 함수는 아이템의 `pNextItem`을 바꾸지 않는다. `changeToSelectedStash`가 목록을 이어서 넣을 수 있는 이유가 이것이다.
  - `D2InvRemoveItem`(#10243)은 next/prev, 격자 연결, `nNodePos`, `nNodePosOther`(PlugY의 `ItemData2`)를 지운다. 격자 칸은 아이템의 현재 좌표(`tGameCoords`, `path->x/y`)로 지운다.
- 아이템 이동은 `changeToSelectedStash_10`과 같은 방식이다.
  - 현재 페이지는 `D2InvRemoveItem`(`ItemData2` 보존)으로 빼고 `D2InvAddItem`으로 넣는다.
  - 다른 페이지는 `ptListItem` 목록에 넣고 `path->x/y`를 쓴다.
- 무기/방어구/부위/클래스 판정은 `D2CheckItemType`(Equiv 체인)으로 한다. 아이템 타입 행은 코드로 찾는다(`loadTypeIndex`, `PlugY.log`에 기록).
- 1.09 (1.09b, 1.09d): 구조가 1.10 이상과 달라서 `IS_109`(`version_D2Common <= V109d`) 분기로 처리한다. 1.10 이상 경로는 그대로다.
  - 1.09 값은 D2Common getter 함수와 txt 필드 테이블(스택에 만드는 BINField 배열)에서 확인했다. 1.09b와 1.09d는 쓰는 함수 27개의 본문, 필드 위치, 레코드 크기가 모두 같다.

  | 항목 | 1.10 이상 | 1.09 |
  |---|---|---|
  | 인벤토리/페이지 목록 | 아이템 | 노드(+04 아이템, +10 next). 인벤토리 +28 첫 노드, +2C 끝, +30 개수 |
  | ItemData / 소켓 인벤토리 | Unit+14 / +60 | Unit+70 / +84 (`shifting` 사용) |
  | quality, flags | ItemData +00, +18 | 같음 |
  | uniqueID / 그림 변형 | ItemData +28 / +49 | `#10732` / `#10727` (+24 / +45) |
  | items.txt | `ItemsBIN` | code +144, ubercode +14C, ultracode +150, invwidth/invheight +1A4/+1A5, type BYTE +1B3 |
  | itemtypes 수 | `SgptDataTables->nbItemTypes` | `#10672` (1.09에서는 `SgptDataTables`를 쓸 수 없다) |
  | runes.txt | `RunesBIN` (0x120) | 0x118 레코드, complete +80, itype BYTE +86, etype BYTE +8C, rune +90 |
  | 세트 | sets.txt 행 (`setItems[].setId`) | setitems.txt 행이 곧 세트이고, uniqueID가 세트 행 번호이다 |

  - 세트 순서(사용자가 정함): 세트끼리는 setitems.txt 행 순서이고, 같은 세트 안에서는 아이템 코드 순서이다.
  - 아이템 이동(`moveStashItems_9`)은 `changeToSelectedStash_9`과 같은 방식이다.
    - 현재 페이지의 노드를 인벤토리 목록에서 직접 떼고 `#10250`으로 격자 칸을 비운다.
    - 1.09의 `D2InvAddItem`은 격자만 채운다. 노드는 `#10242`가 새로 만들어 연결한다. `D2InvRemoveItem`(#10243)은 노드를 해제하지만 격자는 비우지 않는다.
    - 다른 페이지에 놓을 아이템은 `#10328`/`#10331`로 위치를 쓰고 노드를 그 페이지 목록에 연결한다.
  - 1.09 전용 함수 `D2SetPosX`(#10328), `D2SetPosY`(#10331), `D2GetNbItemTypesBIN`(#10672), `D2GetVarGfxIndex`(#10727)는 `Commons/D2Funcs.h`에 있다. 다른 버전 열은 `00000`이다.
- `sortStashCore.h`는 게임 함수를 쓰지 않는다. Linux에서 `tools/sort_stash_test.cpp`로 빌드해 JS 결과와 비교할 수 있다.
  ```
  g++ -o sort_stash_test tools/sort_stash_test.cpp
  ./sort_stash_test [gridW gridH maxPages] < items.txt
  ```

## 바이너리 분석 방법

- 사용자가 올린 파일로 버전을 확인한다.
  - `D2Client.dll`에는 버전 리소스가 없다. 타임스탬프로 구분한다(1.09b 2001-08-16, 1.09d 2001-11-30, 1.13c 2010-03-09, 1.13d 2011-10-18). 그리고 PlugY에 이미 있는 그 버전 주소의 바이트가 맞는지 대조한다.
    - `GET_VERSION`(`D2wrapper.cpp`)의 판별 값도 확인한다. 예: D2Client +17F, D2Common +10CA, D2Game +1092.
  - `Game.exe` 1.14d는 FileVersion 1.14.3.71이다.
- 디스어셈블: `objdump -d -M intel --no-show-raw-insn D2Client.dll > asm.txt`. 이 파일들은 파일 오프셋이 RVA와 같다.
- 위치 찾기에 쓴 단서:
  - 문자열 `%s\ui\panel\bank`, `%s\ui\panel\TradeStash`와 그 이미지 변수를 참조하는 코드
  - 클래식/LoD 분기 상수: 골드 클릭 판정 `-0x1c7`/`-0x103`, 골드 그리기 `-0x1b8`/`-0xf4`, "Gold Max" `-0x19f`/`-0xdb`
  - Gold Max 문자열 인덱스 `0xfd3`
  - 1.09, 1.14d는 D2isLODGame 함수 호출(`call` 대상)
- 1.09 구조체 확인에 쓴 단서:
  - D2Common의 작은 export getter(예: `#10719` page → ItemData+0x41). D2MOO(1.10)의 ordinal 주석과 맞춰 보면 이름을 알 수 있다. 1.09와 1.10은 같은 ordinal이 많다.
  - txt 필드 오프셋은 `D2CompileTxtFile` 앞에서 스택에 만드는 BINField 배열(이름, 형식, 길이, 오프셋)에서 읽는다.
  - 1.09b와 1.09d는 절대 주소를 가리고 명령어를 비교해서 같은지 확인한다.
- 패치를 추가하거나 바꾸면, 빌드하기 전에 반드시 실제 파일로 시뮬레이션한다:
  ```
  tools/simulate_patches.py --109b <1.09b D2Client.dll> --109d <1.09d D2Client.dll> --113c <1.13c D2Client.dll> --113d <1.13d D2Client.dll> --114d <1.14d Game.exe>
  ```
  - 스크립트의 패치 표와 `BigStash.cpp`의 R8 값이 다르면 실패로 나온다. 둘을 함께 고친다.

## 인게임 테스트 목록

모두 통과했다(1.13c, 1.13d, 1.14d, 그리고 PR #13 뒤 1.09b, 1.09d). 창고 관련 코드를 바꾸면 다시 확인한다.

0. 준비
   - 세이브 폴더를 백업한다.
   - `ActiveLogFile=1`, `ActiveCheckMemory=1`
   - 테스트용 캐릭터: 클래식 SC, 클래식 HC, LoD
1. 실행
   - 오류창 없이 메인 화면까지 뜬다.
   - `PlugY.log`에 BigStash 클래식 패치 줄이 있고 `Error`가 없다.
2. 클래식 + BigStash 켬: 배경, 10x10 격자, 기존 6x4 아이템 위치, 페이지 이름과 팝업, 골드 필드 표시와 클릭(인출 창), 골드 넣기/빼기 버튼, 하단 버튼, 닫기, 800x600과 640x480
3. 클래식 + BigStash 끔: 원래 클래식 화면, 페이지 이름 위치, 골드 버튼 위치, 하단 버튼, 골드 클릭
4. 클래식 멀티페이지: 페이지 저장과 재접속, Shift 이동, 인덱스, `/rp /ip /dp /sp /tp /setindex /resetindex`, `.d2x` 생성, 세트 아이템 녹색 표시
5. 공유 창고 분리: `_CLASSIC_`/`_CLASSIC_HC_` 생성, LoD와 섞이지 않음, 클래식끼리 공유, 공유 골드, `OpenSharedStashOnLoading`
6. 클래식 스탯/스킬: 스탯 되돌리기(KeyUsed, Shift), 팝업, 스킬 되돌리기 버튼, `/page 1`~`5`, `/page 6`은 기본 스탯 화면
7. 저장: 자동 백업 폴더 내용, `/save`, `/renamechar`
8. TCP/IP: 클래식 개인 창고 저장, `ActiveSharedStashInMultiPlayer=1`일 때 `_CLASSIC_` 저장
9. LoD 회귀: 창고(BigStash 켬/끔), 기존 `.d2x`와 `_LOD_` 파일, 스탯/스킬/룬워드 페이지
10. 예외: 클래식 → LoD 전환 캐릭터, LoD 미설치 환경의 BigStash 배경

창고 정렬 버튼:

- 1.13c, 1.13d, 1.14d에서 싱글/TCP/IP, 클래식/확장팩, 개인/공유 창고 조합을 모두 확인했다(PR #8 뒤 사용자 보고).
- 1.09b, 1.09d에서도 통과했다(PR #13 뒤 사용자 보고).
- 640x480(11번)과 `MaxPersonnalPages`/`MaxSharedPages` 제한(17번)도 1.14d LoD에서 확인했다(PR #11 뒤 사용자 보고).
  - 640x480에서는 버튼이 HP 구체 뒤에 가려지고 클릭은 된다.
  - 17번: 개인 창고 145개(3페이지 제한)는 빈칸 없이 3페이지에 배치됐고, 155개(3페이지 제한)와 10942개(5페이지 제한)는 아무것도 옮기지 않았다. 공유 창고 145개(3페이지 제한)는 서버와 클라이언트 체크섬이 같았고, 148개는 아무것도 옮기지 않았다.
    - 개인 창고 시험은 PR #7 빌드의 DLL로 했다. PR #8은 이 페이지 제한 로직(`sortStashPlan`)을 바꾸지 않았고, 공유 창고 시험은 최신 빌드로 했다.

11. 버튼: BigStash 켬일 때만 보인다. 그림(보통/눌림), 팝업, 800x600 위치가 다른 버튼이나 닫기 버튼과 겹치지 않는다. 640x480에서는 HP 구체 뒤에 가려지지만 클릭은 된다.
12. 정렬 결과: 품질 블록 순서, 그룹마다 새 페이지, 같은 아이템끼리 한 줄, 룬/보석/참 순서, 세트는 세트별 페이지, 룬워드는 룬워드별 페이지, 같은 그림의 링/아뮬렛/참/주얼이 붙어 있음
13. 페이지: 모자라면 페이지가 생긴다. 이름과 인덱스가 지워진다. 페이지를 오가도 위치가 같다.
14. 저장: `/save`와 재접속 후 위치가 유지된다. 남는 빈 페이지가 없어진다.
15. 공유 창고와 클래식(SC/HC), LoD 각각
16. TCP/IP 클라이언트: 정렬 후 화면과 저장 결과가 같다.
17. `MaxPersonnalPages`를 작게 하면 그룹 구분 없이 배치된다. 그래도 모자라면 아무것도 옮기지 않는다.
18. 첫 정렬 뒤 `PlugY.log`에 `sortStash : item type weap = ...` 줄이 있고, 값이 -1이 아니다(warl만 -1).

## 남은 것

- 1.10 - 1.12에서 BigStash를 켜면 클래식 창고는 원래 6x4 화면 그대로다. 정렬 버튼도 나오지 않는다(LoD 포함). 해당 버전 D2Client 분석이 필요하다.
  - 순서(사용자가 정함): 1.10 → 1.11 → 1.11b → 1.12. 한 버전을 작업하고 인게임에서 확인한 뒤 다음 버전으로 간다.
  - 저장, 공유 창고, 스탯/스킬은 버전 무관 코드라 동작할 것으로 보지만 테스트하지 않았다.
  - 1.10의 구조체는 PlugY 정의(1.10 기준)와 같을 것으로 보지만, 정렬에 쓰는 값은 바이너리로 확인한다. `D2Funcs.h`의 1.09 전용 함수는 1.10 열이 `00000`이다.
- 1.09b Game 구조체는 난이도(+0x4D)와 확장팩 여부(+0x50)가 다른 버전보다 0x20 앞에 있다. 다른 Game 필드를 1.09b에서 쓸 때도 확인한다.
- 독일어/프랑스어/중국어 설치 README(`PlugYInstaller/*.txt`)는 클래식 지원 내용으로 갱신하지 않았다.

## 이력

- PR #1: Classic 캐릭터 지원 (저장/로드, 공유 창고 분리, UI 체크 제거, 1.13c BigStash 클래식 화면)
- PR #2: MSVC CI 빌드 추가, `backupSaveFiles` 선언 누락으로 생긴 링크 오류 수정
- PR #3: 1.13c Fog #10227 패치 위치 수정 (`8CC05` → `8CC06`)
- PR #4: 1.13d / 1.14d BigStash 클래식 화면
- PR #5: `README.md` 재작성
- PR #6: `CLAUDE.md`와 검증 도구(`tools/`) 추가
- PR #7: 창고 정렬 버튼 (`SortStash.cpp`, `sortStashCore.h`, `SortBtns.dc6`), CI 아티팩트에 `PlugY\` 데이터 파일 포함
- PR #8: 창고 정렬을 클라이언트도 직접 계산하고 서버는 체크섬만 보냄 (TCP/IP 큰 창고에서 메시지가 유실되던 문제 수정)
- PR #9: PR #8 뒤 창고 정렬 인게임 테스트 결과를 `CLAUDE.md`에 기록
- PR #10: `README.md`에 창고 정렬 버튼 추가
- PR #11: 포크 이후 변경 전체 검토 후 정리 (쓰지 않는 capstone `.whl` 삭제, 이 이력에 PR #9, #10 추가, `README.md` 정렬 버튼 조건에 `ActiveMultiPageStash=1` 추가)
- PR #12: 창고 정렬의 640x480 버튼 위치와 페이지 제한 테스트 결과를 기록 (640x480에서는 버튼이 HP 구체 뒤에 가려지지만 그대로 두기로 함, 페이지 제한 시 빈칸 없는 재배치를 확정)
- PR #13: 1.09b / 1.09d BigStash 클래식 화면과 창고 정렬, 1.09b 싱글 클래식 공유 창고 판정 수정(Game+0x50)
- PR #14: PR #13 작업 내용과 1.09b / 1.09d 테스트 결과를 `CLAUDE.md`, `README.md`에 기록, 작업 규칙 추가(수정 전 보고와 허가, 문서는 마지막에 갱신)
