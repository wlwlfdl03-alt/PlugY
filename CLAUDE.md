# PlugY (Diablo II Classic 지원 포크)

원본 PlugY(Yohann NICOLAS, ChaosMarc/PlugY)의 포크다. LoD 전용이던 PlugY 기능을 Diablo II Classic 캐릭터에서도 쓸 수 있게 하는 것이 목표다.

- LoD: 1.09b - 1.13d, 1.14d (원본과 같음)
- Classic: 1.13c, 1.13d, 1.14d (인게임 테스트 완료)

## 작업 규칙 (사용자가 정한 것)

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
- `.github/workflows/build.yml`가 push, PR, 수동 실행 때 windows-2022에서 빌드하고 `PlugY` 아티팩트(`PlugY.dll`, `PlugY.pdb`)를 올린다. 사용자는 이 아티팩트로 테스트한다.
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
- 싱글은 `PCGame->isLODGame`으로 클래식/LoD를 판별한다.
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

### BigStash 클래식 화면 (`BigStash.cpp`, 1.13c / 1.13d / 1.14d만)
- inventory.txt 행(컴파일 시 "Expansion" 구분 행은 빠진다):
  - "Bank Page 1"(클래식 창고) = 8, 800x600 = 24
  - "Big Bank Page 1"(LoD) = 12, 800x600 = 28
- 클래식 행은 패치가 설치된 버전에서만 10x10으로 바꾼다(`active_classicBigStash`).
- 클래식 창고 배경은 `%s\ui\panel\bank`이다. LoD 배경 `%s\ui\panel\TradeStash`는 Fog #10227(LoD 설치 여부)이 참일 때만 불러온다. PlugY는 이 TradeStash 경로만 `PlugY\TradeStash`로 바꾼다.
- 창고 화면의 레이아웃 분기(배경, "Gold Max" 위치, 골드 필드 그리기, 골드 필드 클릭 판정)가 IsLodGame 대신 `stashLodLayout`(= `isLodStashLayout()`)을 보게 한다.
  - 1.13c, 1.13d: IsLodGame을 읽는 명령의 주소 피연산자를 `&stashLodLayout`으로 바꾼다. 값은 창고를 그리기 직전(`caller_updateStashLayout`)과 클릭 판정 시작(`caller_isOnStashGoldField`)에 갱신한다.
  - 1.14d: 창고 코드가 IsLodGame 변수를 직접 읽지 않고 함수 `Game.0044DCC0`을 호출한다. 이 호출 4곳을 `caller_isLodStashLayout_114`로 바꾼다.
  - 세 버전 모두 Fog #10227 호출을 `caller_loadTradeStash`로 바꿔서, LoD가 없어도 PlugY 배경을 불러온다. 1.14d에서는 이 호출 대상이 Game.exe 내부 함수 `00408F20`이다.
- 닫기 버튼 위치 함수(1.13c `6FB3BEB0`/`6FB3BEE0`)는 다른 패널과 함께 쓰는 코드라 건드리지 않았다. 클래식은 LoD보다 X로 3px, Y로 1px 다르다.

| 패치 | 1.13c | 1.13d | 1.14d |
|---|---|---|---|
| Fog #10227 호출 피연산자 | 8CC06 (→BF8C) | 943E6 (→BEA0) | 89E9D (→8F20) |
| 기존 배경 경로 패치 `PUSH 104` | 8CC1C | 943FC | 89EB5 |
| bank 이미지 변수 | 11BC74 | 11CB64 | 3BCC2C (참고) |
| 레이아웃 갱신 `MOV EAX,[bank]` | 998D5 | 9DCA5 | - |
| 배경 선택 | 998E4 | 9DCB4 | 8F19C (call) |
| "Gold Max" 위치 | 999FB | 9DDCB | 8F2B1 (call) |
| 골드 필드 그리기 | 8D14C | 9119C | 8820D (call) |
| 골드 필드 클릭 판정 | 8B6B0 (함수 시작) | 8FA30 | 89921 (call) |
| IsLodGame 변수 | 119854 | 1087B4 | 3A04F4 |

## 바이너리 분석 방법

- 사용자가 올린 파일로 버전을 확인한다.
  - `D2Client.dll`에는 버전 리소스가 없다. 타임스탬프로 구분한다(1.13c 2010-03-09, 1.13d 2011-10-18). 그리고 PlugY에 이미 있는 그 버전 주소의 바이트가 맞는지 대조한다.
  - `Game.exe` 1.14d는 FileVersion 1.14.3.71이다.
- 디스어셈블: `objdump -d -M intel --no-show-raw-insn D2Client.dll > asm.txt`. 이 파일들은 파일 오프셋이 RVA와 같다.
- 위치 찾기에 쓴 단서:
  - 문자열 `%s\ui\panel\bank`, `%s\ui\panel\TradeStash`와 그 이미지 변수를 참조하는 코드
  - 클래식/LoD 분기 상수: 골드 클릭 판정 `-0x1c7`/`-0x103`, 골드 그리기 `-0x1b8`/`-0xf4`, "Gold Max" `-0x19f`/`-0xdb`
  - Gold Max 문자열 인덱스 `0xfd3`
- 패치를 추가하거나 바꾸면, 빌드하기 전에 반드시 실제 파일로 시뮬레이션한다:
  ```
  tools/simulate_patches.py --113c <1.13c D2Client.dll> --113d <1.13d D2Client.dll> --114d <1.14d Game.exe>
  ```
  - 스크립트의 패치 표와 `BigStash.cpp`의 R8 값이 다르면 실패로 나온다. 둘을 함께 고친다.

## 인게임 테스트 목록

모두 통과했다(1.13c, 1.13d, 1.14d). 창고 관련 코드를 바꾸면 다시 확인한다.

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

## 남은 것

- 1.09b - 1.12에서 BigStash를 켜면 클래식 창고는 원래 6x4 화면 그대로다. 해당 버전 D2Client 분석이 필요하다.
  - 저장, 공유 창고, 스탯/스킬은 버전 무관 코드라 동작할 것으로 보지만 테스트하지 않았다.
- 독일어/프랑스어/중국어 설치 README(`PlugYInstaller/*.txt`)는 클래식 지원 내용으로 갱신하지 않았다.

## 이력

- PR #1: Classic 캐릭터 지원 (저장/로드, 공유 창고 분리, UI 체크 제거, 1.13c BigStash 클래식 화면)
- PR #2: MSVC CI 빌드 추가, `backupSaveFiles` 선언 누락으로 생긴 링크 오류 수정
- PR #3: 1.13c Fog #10227 패치 위치 수정 (`8CC05` → `8CC06`)
- PR #4: 1.13d / 1.14d BigStash 클래식 화면
- PR #5: `README.md` 재작성
