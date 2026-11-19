# UI 정적 전수 조사 (Part 2)

## 방법

- `run/prim/sa.py`는 codemap의 Thumb 함수 2,339개를 모두 역어셈블해 다음을 뽑는다(`sa.json`).
  - 직접 `bl`, API 호출(GOT→표→오프셋 추적), GOT/GOTOFF 참조(문자열, 함수 포인터), vtable 저장.
- `run/prim/sweep.py`는 그 위에서 동작한다.
  - **프리미티브**(primitives.md의 글자 9개, 창 장식 12개, 스프라이트 17개, 함수 이름은 표의 열과 같다)에 직접 호출로 **3단계 안에** 닿는 함수를 모은다. 프리미티브 내부(0x1f00~0x7500)는 뺀다.
  - 화면을 정하는 방법: 호출자를 위로 따라가며(직접 호출자 + vtable 메서드의 생성자 + 함수 포인터 소유자) 가장 가까운 **기준점**(장면 표 24개, 메뉴 화면 클래스 생성자, 전투, 이벤트 스크립트, 필드 객체 클래스. `run/prim/anchors.json`)을 찾는다.
  - 거리가 같은 기준점이 여럿이면 모두 적는다.
- 결과는 **243개 함수**다(`run/prim/sweep.json`, `sweep1.txt`).
- 한계:
  - vtable로만 불리고 생성자가 기준점에 닿지 않는 함수, 함수 포인터 콜백 안의 간접 호출은 화면이 "미확인"이나 넓은 이름(scene12 등)으로 남는다.
  - 기준점 이름은 추정을 포함한다(장면 번호 → 이름은 primitives.md §1).
  - 「직접 부르는 프리미티브」가 괄호인 줄은 직접 그리지 않고 3단계 안에서 닿는 것만 적었다.

## 군집 요약

| 군집 | 함수 수 | 주요 함수 | 상태 |
|---|---|---|---|
| 타이틀 메뉴(장면 4) | 15 | ebc4 메뉴, f580/f834 OST, f8c0 환경설정, fc9c 게임문의, fe68 정보이용료, ffbc OST 설명, 100a0 도움말, 13060 에픽한마디, 134cc 랭킹(서버) | 담당: 타이틀 |
| 로고/타이틀 배경(장면 1, 2) | 3 | e5e4, e622(호출자 없음, 함수 포인터), e85c | **미배정(타이틀 담당 확인 필요)** |
| 로딩(5, 18), 오류(0), 첫 실행(22) | 6 | 10410 「Loading.....」, e588, 10ef0 「속도최적화」 | 단순 |
| 필드(6, 7) + 필드 장면 그리기 | ~20 | c958, cab0, 2ee2c 맵, 12b18 「【이착륙:0번키】」(비공정 안내), 12dd8, 26274 | 담당: 필드 HUD |
| 필드 메뉴 | 60 | 2faa4 분배, 메인 310e8(31238 머리, 31398 파티 요약, 31474, 314d0, 31508 지역 이름), 아이템 31b18, 장비 32778, 스킬 33d94, 퀘스트 338b8, 전투동료 34cb8, 시스템 35440/34f48, 상점 360b0, 미확인 3312c | 담당: 필드 메뉴(상점 일부는 상점 담당) |
| 공용 팝업 | 6 | 23578(팝업 기반 그리기, 글 상자 4개), 232c8, 234a0/234c4(반투명 바탕), 36d74/36eb0(4dbec) | **36d74 미확인** |
| 이벤트 스크립트 UI | ~45 | 38e18 실행기, 3a4a4 명령(7.5KB), 37388/3716c 대사창(선, 합성 프레임, 글 상자), 13e48 외곽선 가운데 글, bfe8 「#:Fast *:Exit」, 39840 여관 「숙박비」, 398d4 저장, 39914 헌화, 39954 꽃, 3ccfc 「전투하시겠습니까/경품교환/대화하기/나가기」, 3cdc0 슬롯머신/코인, 3cf10 도움말 선택, 3d018 다시하기, 14a68/147e0 화면 효과 | 담당: 대사(일부). **선택지/여관/헌화/카지노 메뉴는 배정 필요** |
| 전투 | ~45 | 1bcd4 그리기, 1c1ac(창 4, 글 상자), 1e0d8/1e894/1ee3c 상태 바·게이지, 1dfec 메시지 창, 1cf38 「LEVEL UP」, 1f3d0/1fe38 스킬 명령창, 2fffc 구분선, 18f7c~196c0 유닛 | 담당: 전투 |
| 미니게임(15, 16) | 21 | 26db8, 276b8 「Score/준비~/게임 끝」, 27bf0 「COIN」(트리플 슬롯), 279a4/27a4c, 275f0 「끝내시겠습니까?」 | **미배정** |
| 장면 21(지도?) | 3 | 10b8c(창 2, 게이지 2, 글 상자), 2f06c 미니맵, 302b4 키 도움말 바(공용) | **미배정. 정체 확인 필요** |
| 미확인 | 8 | e318(←23cd4 필드 객체), 12efc(←64f8 pause, 일시정지 화면?), 18f5c, 22478(리소스), b220/b354/bba0(초기화, 장면 실행), 26c58(장면 19) | **미배정** |

## 미확인 / 배정이 필요한 군집

1. **필드 메뉴 4db90 화면**(`32fc8` 생성, 그리기 `3312c`, 1.5KB, 비트맵 숫자 17개, 스프라이트 프레임 13개, 문자열 없음): 상태(능력치) 화면으로 추정한다. 필드 메뉴 담당이 메인 메뉴의 몇 번째 항목인지 확인할 것.
2. **장면 21 `10b8c`**(+`2f06c` 미니맵): 지도 화면으로 추정한다. 어떤 키로 들어가는지 모른다.
3. **팝업 4dbec**(`36c90` ← `11740`, 그리기 `36d74`/`36eb0`: fillRect 9개, 스프라이트 프레임 6개).
4. **팝업 4dbcc**(`370bc` ← `b220`, 슬롯 `37100`)와 선택지 팝업 4dbdc(`377c4/377f8` ← 스크립트 `399bc`, `3ccfc`): 공용 예/아니오·선택 팝업. 대사 담당과 함께 정리할 것.
5. **이벤트 스크립트의 시설 메뉴들**(여관 39840, 저장 398d4, 헌화 39914/39954, 카지노·경품 3ccfc/3cdc0/3cf10/3d018, 저장 슬롯 선택 3a4a4 「저장슬롯선택」, 기록 비교 「최고기록:」).
6. **미니게임**(벼베기, 트리플 슬롯; 장면 15, 16, `26db8~28114`) 전체.
7. **로고/타이틀 배경**(장면 1, 2: `e5e4`, `e622`, `e85c`)과 일시정지로 추정되는 `12efc`.
8. **랭킹/네트워크 화면** `134cc`(서버가 없어 실제로는 오류 메시지만 나온다. 「CLR : 나가기」).

## 마스터 목록

화면 열의 "/" 뒤는 하위 화면이다. 근거 열: 「」 참조 문자열, vtable 슬롯, ← 직접 호출자(최대 4개).

| 오프셋 | 크기 | 화면 | 직접 부르는 프리미티브 | 근거(문자열, vtable, 호출자) |
|---|---|---|---|---|
| `e588` | 84 | 오류 | fillRect(gfx)×1, drawText(xy)×1 | 「프로그램오류!!!」 |
| `e5e4` | 614 | 로고(추정) | fillRect(gfx)×2, sprAnimFx×3 |  |
| `e85c` | 844 | 타이틀 배경(추정) | fillRect(gfx)×1, sprComposite×8, fillRect×4 |  |
| `ebc4` | 2440 | 타이틀 메뉴 | fillRect×2, drawRect×5, sprComposite×9 | 「게임시작」「게임정보」「환경설정」「네트워크」「게임종료」「이어하기」「새게임」「트리플슬롯」「저들의영웅」「만든사람들」「에픽한마디」「게」; ← bba0,12efc |
| `f580` | 414 | 타이틀 메뉴 | drawRect×1, sprComposite×3, textBox(char*)×1, fillRect×3 | ← ebc4,f834 |
| `f834` | 126 | 타이틀 메뉴 | (textBox(char*), sprComposite, fillRect …) | 「메인테마」「월드테마」「이오의테마」「신전테마」「모험의테마」「fee le monde 히데오의 '여행을떠나며'. 에픽의 메인테마곡.」; ← ebc4 |
| `f8c0` | 988 | 타이틀 메뉴 | sprComposite×8, drawText(xy)×7, measure×4, gaugeBox×1 | 「환경설정」「저장」「이전」「퀵모드」「%d」「속도」「사운드」「전투」; ← ebc4 |
| `fc9c` | 436 | 타이틀 메뉴 | fillRect×3, sprComposite×4 | 「게임문의」; ← ebc4 |
| `fe68` | 118 | 타이틀 메뉴 | textBox(x,y,w,h)×1 | 「정보이용료」「네트워크사용량에 따라 기본통신료외 1패킷(512byte)당 0원의 추가정보 이용료가 부과됩니다.」; ← ebc4 |
| `fef0` | 194 | 타이틀 메뉴 | measure×1, sprComposite×3, drawText(xy)×1 | 「이전」; ← fc9c,fe68,ffbc |
| `ffbc` | 142 | 타이틀 메뉴 | textBox(x,y,w,h)×1 | 「메인테마」「월드테마」「이오의테마」「신전테마」「모험의테마」「fee le monde 히데오의 '여행을떠나며'. 에픽의 메인테마곡.」; ← ebc4 |
| `100a0` | 412 | 타이틀 메뉴 | measure×1, sprComposite×5, drawText(xy)×1 | 「도움말」「이전」「게임속으로」; ← ebc4 |
| `10294` | 310 | 타이틀 메뉴 | drawText(xy)×2 | ← 100a0,10258,27144,274c8 |
| `12598` | 274 | 타이틀 메뉴 | fillRect(gfx)×1 | ← b354,cd80,ebc4,11228 |
| `13060` | 802 | 타이틀 메뉴 | textBox(x,y,w,h)×1, measure×1, sprComposite×7, drawText(xy)×1 | 「에픽한마디」「이전」; ← ebc4 |
| `261d4` | 160 | 타이틀 메뉴 | fillRect(gfx)×1 | ← ebc4,f580,234a0,234e4 |
| `103f4` | 28 | 로딩 | fillRect(gfx)×1 | ← 10458,108b8,111b8 |
| `10410` | 62 | 로딩 | measure×1, drawText(xy)×1 | 「Loading.....」; ← 103f4 |
| `10458` | 52 | 로딩 | (fillRect(gfx), drawText(xy), measure …) |  |
| `108b8` | 60 | 로딩 | (fillRect(gfx), drawText(xy), measure …) |  |
| `111b8` | 74 | 로딩 | (fillRect(gfx), drawText(xy), measure …) | ← 108b8 |
| `10ef0` | 430 | 첫 실행 최적화 | fillRect(gfx)×3, fillRect×1, drawRect×1, gaugeBox×1, drawText(xy)×1, textBoxMain×2 | 「속도최적화...」「게임종료 후 재시작해야 합니다.⏎게임속도가 느릴경우, 메인메뉴 환경설정에서 퀵모드를 On으로 변경하세요.」「」 |
| `c958` | 212 | 필드 | fillRect(gfx)×1 | ← cbec,cbf8,104e0,1054c |
| `cab0` | 310 | 필드 | fillRect(gfx)×1 | ← c958,cdf0,109b8 |
| `104e0` | 106 | 필드 | (fillRect(gfx), drawText(xy), measure …) |  |
| `1054c` | 294 | 필드 | (fillRect(gfx), drawText(xy), measure …) |  |
| `11228` | 154 | 필드 | (fillRect(gfx) …) | ← 104e0 |
| `1170c` | 52 | 필드 | (slotAnim …) | ← cab0 |
| `12b18` | 118 | 필드 | slot170×1, measure×1, drawText(xy)×1 | 「【이착륙:0번키】」; ← c958 |
| `12dd8` | 236 | 필드 | slotAnim3×1, slotAnim×1 | ← c958 |
| `14028` | 226 | 필드 | slotAnim3×1 | ← cab0,24220 |
| `16398` | 50 | 필드 | slotFrame×1 | vtable vt4d9bc[16] |
| `17bd8` | 20 | 필드 | slotAnim×1 | vtable vt4d9bc[10] |
| `17bec` | 20 | 필드 | slotAnim3×1 | vtable vt4d9bc[11] |
| `17c00` | 200 | 필드 | (slotAnim3 …) | vtable vt4d9bc[12] |
| `23b20` | 318 | 필드 | slotAnim×1, slotAnim2×1 | vtable vt4da10[6] vt4dafc[6] |
| `24220` | 320 | 필드 | slotFrame×1, slotAnim×2 | vtable vt4d944[6] vt4da84[6]; ← 25060,25200 |
| `25034` | 44 | 필드 | slotAnim×1 | ← 1170c |
| `25060` | 16 | 필드 | (slotAnim, slotFrame, slotAnim3 …) | vtable vt4dac0[6] |
| `25200` | 160 | 필드 | slotAnim×1 | vtable vt4d980[6] |
| `26274` | 102 | 필드 | drawRect(gfx)×2, fillRect(gfx)×1 | ← 12b18,234a0,234c4 |
| `2df44` | 154 | 필드 | blitFx×1, blitH×1, blitHV×1, blitV×1, blit×1 | ← 2e174,2e1ec,2e2f8 |
| `2e068` | 62 | 필드 | (blit, blitV, blitH …) | ← 27a4c,2ee2c,381a0,38418 |
| `2e100` | 106 | 필드 | (blit, blitV, blitH …) | ← 2f170 |
| `2e174` | 114 | 필드 | (blit, blitV, blitH …) | ← 2e068,2e0a8,2e100,2ee2c |
| `2e1ec` | 188 | 필드 | (blit, blitV, blitH …) | ← 2ee2c |
| `2ec68` | 126 | 필드 | (fillRect(gfx) …) | ← 2ee2c,38418 |
| `2ee2c` | 566 | 필드 | fillRect×2, fillRect(gfx)×3 | ← cab0,12dd8,20890 |
| `2f7c4` | 196 | 필드 | fillRect(gfx)×4 | ← 2ec68 |
| `109b8` | 54 | 필드 메뉴 | (fillRect(gfx), fillRect, slotAnim3 …) |  |
| `2faa4` | 74 | 필드 메뉴 | fillRect×1 | ← 109b8 |
| `11e48` | 130 | 필드 메뉴/메인 | slotFrame×1 | ← 11ed0 |
| `11ed0` | 102 | 필드 메뉴/메인 | (slotFrame, drawNumber(bitmap) …) | ← 314d0,356e8 |
| `3005c` | 28 | 필드 메뉴/메인 | gaugeBox×1 | ← 30078 |
| `30078` | 40 | 필드 메뉴/메인 | (gaugeBox, fillRect, drawRect …) | ← 310e8 |
| `310e8` | 304 | 필드 메뉴/메인 | fillRect×1, drawText(xy)×1 | vtable vt4db6c[1] |
| `31238` | 344 | 필드 메뉴/메인 | window×1, slotFrame×6, sprComposite×2 | ← 310e8 |
| `31398` | 210 | 필드 메뉴/메인 | window×1, gaugeBox×1 | ← 310e8 |
| `31474` | 88 | 필드 메뉴/메인 | window×1, slotFrame×1 | ← 310e8 |
| `314d0` | 54 | 필드 메뉴/메인 | window×1 | ← 310e8 |
| `31508` | 158 | 필드 메뉴/메인 | window×1, textBox×1 | ← 310e8 |
| `31b18` | 432 | 필드 메뉴/아이템 | window×1, slotFrame×1, gaugeBox×1, textBox(x,y,w,h)×1 | 「소비물품」; vtable vt4db78[1] |
| `31cec` | 388 | 필드 메뉴/아이템 | window×1, gaugeBox×1, textBoxMain×1, measure×1, slotFrame×1, drawNumber(bitmap)×1 | ← 31b18 |
| `31e74` | 694 | 필드 메뉴/아이템 | window×4, textBoxMain×2, measure×1, drawNumber(bitmap)×1, slotFrame×4, drawText(xy)×5, gaugeBox×2 | 「판매하시겠습니까?」「예」「아니오」「판매개수」「판매금액」; ← 31b18 |
| `32148` | 484 | 필드 메뉴/아이템 | window×2, textBoxMain×1, measure×1, drawNumber(bitmap)×1, slotFrame×3, gaugeBox×1 | ← 31b18 |
| `32338` | 472 | 필드 메뉴/아이템 | marqueeText×1, drawRect×1, window×2, slotFrame×1 | ← 31b18 |
| `2fe7c` | 156 | 필드 메뉴/장비 | slotFrame×1 | ← 308a4,32778,34a08 |
| `301c4` | 48 | 필드 메뉴/장비 | slotFrame×1 | ← 31cec,31e74,32778,36250 |
| `32778` | 1666 | 필드 메뉴/장비 | window×4, slotFrame×8, drawNumber(bitmap)×6, textBoxMain×5, gaugeBox×2, measure×1 | 「무기류」「장착불가 Lv%d」「소지한 장비가 없습니다.」; vtable vt4db84[1] |
| `32f48` | 112 | 필드 메뉴/장비 | slotFrame×1, drawNumber(bitmap)×1 | ← 32778 |
| `33d94` | 96 | 필드 메뉴/스킬 | (sprComposite, window, textBoxMain …) | vtable vt4db9c[1] |
| `33dfc` | 220 | 필드 메뉴/스킬 | window×1, textBoxMain×2 | ← 33d94 |
| `33ee4` | 10 | 필드 메뉴/스킬 | (sprComposite, sprFrame, textBoxMain …) | ← 33dfc,33ef0,3403c |
| `33ef0` | 306 | 필드 메뉴/스킬 | window×1, gaugeBox×2, textBox(char*)×1, slotFrame×1 | 「필 살 기 x」「조합 스킬 x」「일반 스킬 x」; ← 33d94 |
| `3403c` | 1246 | 필드 메뉴/스킬 | window×2, gaugeBox×4, textBoxMain×5, slotFrame×3 | 「사용」「3,6,9 단축키」「사용가능」; ← 33d94 |
| `34528` | 582 | 필드 메뉴/스킬 | window×2, slotFrame×5, textBoxMain×1, drawNumber(bitmap)×2, gaugeBox×1 | 「票替濁什戱凹典訂傳」「MP부족」; ← 33d94 |
| `34a08` | 376 | 필드 메뉴/스킬 | window×1, slotFrame×7, drawNumber(bitmap)×5 | ← 33d94 |
| `338b8` | 288 | 필드 메뉴/퀘스트 | window×2, gaugeBox×1, textBoxMain×2, slotFrame×1, drawNumber(bitmap)×2 | 「퀘스트」「등록된 퀘스트가 없습니다.」; vtable vt4dbfc[1] |
| `308a4` | 382 | 필드 메뉴/전투동료 | sprComposite×1, slotFrame×4, drawNumber(bitmap)×4 | ← 32148,34528,34cb8 |
| `30a38` | 580 | 필드 메뉴/전투동료 | sprComposite×1, slotFrame×3, drawNumber(bitmap)×3, drawRect×2, drawLine(gfx)×4 | ← 31398,34cb8 |
| `34cb8` | 352 | 필드 메뉴/전투동료 | window×3, textBoxMain×1, gaugeBox×1, slotFrame×2, sprComposite×1 | 「전투동료설정」; vtable vt4dba8[1] |
| `10258` | 50 | 필드 메뉴/시스템(도움말·저장·옵션) | (drawText(xy) …) | ← 34f48 |
| `34f48` | 320 | 필드 메뉴/시스템(도움말·저장·옵션) | window×2, textBoxMain×1, sprComposite×2 | 「도움말」; vtable vt4dbb4[2] |
| `35440` | 46 | 필드 메뉴/시스템(도움말·저장·옵션) | (slotFrame, drawRect, textBoxMain …) | vtable vt4dbb4[1] |
| `35470` | 296 | 필드 메뉴/시스템(도움말·저장·옵션) | window×1, gaugeBox×2, slotFrame×1, textBoxMain×1, drawRect×1 | 「저장/불러오기」「종료하시겠습니까?」; ← 35440 |
| `355ac` | 296 | 필드 메뉴/시스템(도움말·저장·옵션) | window×2, textBoxMain×3, gaugeBox×1, slotFrame×1 | 「저장/불러오기」「저장」「불러오기」; ← 35440 |
| `356e8` | 728 | 필드 메뉴/시스템(도움말·저장·옵션) | window×2, textBox(x,y,w,h)×1, measure×1, drawText(xy)×1, drawRect×1, gaugeBox×2, textBoxMain×2, fillRect(gfx)×1 | 「Total time 」「Total.Time 」「저장 중!! 완료까지 종료하면 안됩니다.」「비어있음」; ← 355ac |
| `359f8` | 520 | 필드 메뉴/시스템(도움말·저장·옵션) | window×2, textBoxMain×1, drawText(xy)×3, measure×2, gaugeBox×1, sprComposite×2 | 「환경설정」「%d」「속도」「사운드」「타이핑」「효과」「Off」「On」「보통」「최상」; ← 35440 |
| `35c1c` | 76 | 필드 메뉴/시스템(도움말·저장·옵션) | window×1, textBoxMain×1 | 「옵션」; ← 35470 |
| `37c08` | 228 | 필드 메뉴/시스템(도움말·저장·옵션) | gaugeBox×1, drawText(xy)×1, textBoxMain×3 | 「예」「아니오」; ← 35470,356e8 |
| `360b0` | 388 | 필드 메뉴/상점 | gaugeBox×3, slotFrame×2, drawText(xy)×5 | 「아이템 구입」「아이템 판매」「무기류」「방어구류」; vtable vt4dbc0[1] |
| `36250` | 622 | 필드 메뉴/상점 | textBoxMain×2, slotFrame×6, drawNumber(bitmap)×5 | 「가격」; ← 360b0 |
| `36534` | 626 | 필드 메뉴/상점 | window×2, drawLine(gfx)×2, slotAnim×1, slotFrame×1, drawText(xy)×1, slot170×1 | 「%d개」「코인: %dC」; ← 360b0 |
| `367bc` | 656 | 필드 메뉴/상점 | slotFrame×4, drawText(xy)×6, gaugeBox×1, window×1 | 「구매개수」「소지코인」「구입하시겠습니까?」「예」「아니오」; ← 360b0 |
| `36a84` | 496 | 필드 메뉴/상점 | window×2, gaugeBox×1, textBoxMain×4, slotFrame×2 | 「구매코인개수」「%d」「구매금액 %d」「코인구매」; ← 367bc |
| `3312c` | 1536 | 필드 메뉴/미확인(4db90, 상태?) | window×4, textBoxMain×1, slotFrame×13, drawNumber(bitmap)×17, slot170×1, gaugeBox×1 | vtable vt4db90[1] |
| `23120` | 46 | 공용 팝업 | (textBox(char*), slot170 …) | vtable vt4da58[3] |
| `232c8` | 316 | 공용 팝업 | measure×1 | vtable vt4da48[1] vt4da58[1] |
| `234a0` | 28 | 공용 팝업 | (fillRect(gfx), drawRect(gfx) …) | ← 23578,37388,37894 |
| `234c4` | 30 | 공용 팝업 | slot170×1 | ← 23578 |
| `23578` | 432 | 공용 팝업 | textBox(char*)×4 | vtable vt4da48[3] vt4dbcc[3]; ← 23120 |
| `36d74` | 316 | 공용 팝업 | drawText(xy)×1, measure×1 | vtable vt4dbec[3] |
| `36eb0` | 492 | 공용 팝업 | fillRect(gfx)×9, slotFrame×6 | ← 36d74 |
| `106c8` | 60 | 이벤트 | (drawText(xy), measure, fillRect(gfx) …) |  |
| `10704` | 60 | 이벤트 | (drawText(xy), measure, fillRect(gfx) …) |  |
| `38dbc` | 92 | 이벤트 | (drawText(xy), measure, fillRect(gfx) …) | ← e15c,e318,10674 |
| `bf20` | 190 | 이벤트(스크립트 UI) | (measure …) | ← bfe8,3a4a4 |
| `bfe8` | 584 | 이벤트(스크립트 UI) | fillRect(gfx)×1, measure×1, drawText(xy)×4, slot17c×2 | 「#:Fast」「*:Exit」; ← 38e18 |
| `cbec` | 10 | 이벤트(스크립트 UI) | (fillRect(gfx), drawText(xy), measure …) | ← 39058 |
| `cbf8` | 18 | 이벤트(스크립트 UI) | (fillRect(gfx), drawText(xy), measure …) | ← 134cc,38bc8,38e18,39338 |
| `cd80` | 108 | 이벤트(스크립트 UI) | fillRect(gfx)×1 | ← 3a4a4 |
| `11530` | 98 | 이벤트(스크립트 UI) | (fillRect(gfx), drawLine, slotComposite …) | ← 3c580,3c710 |
| `11594` | 72 | 이벤트(스크립트 UI) | (fillRect(gfx), drawText(xy), measure …) | ← 3c77c |
| `13c58` | 372 | 이벤트(스크립트 UI) | measure×2 | ← bf20,13dcc |
| `13dcc` | 106 | 이벤트(스크립트 UI) | (measure …) | ← bf20 |
| `13e48` | 360 | 이벤트(스크립트 UI) | measure×4, drawText(xy)×3 | ← bfe8 |
| `147e0` | 76 | 이벤트(스크립트 UI) | slot184×1 | ← 14a68 |
| `1482c` | 72 | 이벤트(스크립트 UI) | fillRect×1 | ← 14a30 |
| `14a30` | 56 | 이벤트(스크립트 UI) | (fillRect, fillRect(gfx) …) | ← 3a4a4 |
| `14a68` | 386 | 이벤트(스크립트 UI) | slot180×2 | ← 3a4a4 |
| `234e4` | 126 | 이벤트(스크립트 UI) | drawRect(gfx)×2, fillRect(gfx)×1 | ← 3716c |
| `2e584` | 48 | 이벤트(스크립트 UI) | (slot174 …) | ← d044 |
| `3716c` | 534 | 이벤트(스크립트 UI) | sprComposite×1, textBox×1, measure×1 | ← 37388 |
| `37388` | 660 | 이벤트(스크립트 UI) | slotComposite×1, drawLine×2 | ← 11530 |
| `37894` | 190 | 이벤트(스크립트 UI) | drawText(xy)×2, slotFrame×1 | ← 39614,397d0 |
| `38bc8` | 40 | 이벤트(스크립트 UI) | (fillRect(gfx) …) | ← 3a4a4 |
| `38e18` | 374 | 이벤트(스크립트 UI) | (drawText(xy), measure, fillRect(gfx) …) | ← e318,106c8,10704,38dbc |
| `38f98` | 150 | 이벤트(스크립트 UI) | (fillRect(gfx), fillRect, drawText(xy) …) | ← 38e18 |
| `39038` | 32 | 이벤트(스크립트 UI) | (fillRect(gfx), fillRect, drawText(xy) …) | ← 39460 |
| `39058` | 726 | 이벤트(스크립트 UI) | (fillRect(gfx), fillRect, drawText(xy) …) | ← 38f98,39038 |
| `39338` | 118 | 이벤트(스크립트 UI) | (fillRect(gfx) …) | ← 38e18 |
| `393b0` | 58 | 이벤트(스크립트 UI) | (fillRect(gfx) …) | ← 38e18 |
| `39590` | 104 | 이벤트(스크립트 UI) | (fillRect(gfx) …) | ← 38e18 |
| `39614` | 428 | 이벤트(스크립트 UI) | (fillRect(gfx), drawText(xy), slotFrame …) | ← 38e18 |
| `397d0` | 98 | 이벤트(스크립트 UI) | (fillRect(gfx), drawText(xy), slotFrame …) | 「예」「아니오」; ← 39840,398a4,398d4,39914 |
| `39840` | 86 | 이벤트(스크립트 UI) | (fillRect(gfx), drawText(xy), slotFrame …) | 「숙박비 」「 골드」; ← 3a4a4 |
| `398a4` | 48 | 이벤트(스크립트 UI) | (fillRect(gfx), drawText(xy), slotFrame …) | ← 3a4a4 |
| `398d4` | 54 | 이벤트(스크립트 UI) | (fillRect(gfx), drawText(xy), slotFrame …) | 「저장하겠습니까?」; ← 3a4a4 |
| `39914` | 54 | 이벤트(스크립트 UI) | (fillRect(gfx), drawText(xy), slotFrame …) | 「헌화하겠습니까?」; ← 3a4a4 |
| `39954` | 90 | 이벤트(스크립트 UI) | (fillRect(gfx), drawText(xy), slotFrame …) | 「꽃 」「 골드」; ← 3a4a4 |
| `39a08` | 100 | 이벤트(스크립트 UI) | (fillRect(gfx), drawText(xy), measure …) | ← 38e18 |
| `3a358` | 50 | 이벤트(스크립트 UI) | (fillRect(gfx) …) | ← 39058 |
| `3a42c` | 60 | 이벤트(스크립트 UI) | fillRect×1, fillRect(gfx)×1 | ← 39058 |
| `3a468` | 60 | 이벤트(스크립트 UI) | drawText(xy)×1 | ← 39058 |
| `3a4a4` | 7538 | 이벤트(스크립트 UI) | (fillRect(gfx), fillRect, slot180 …) | 「 획득」「파티 GP 」「획득」「 GP 」「저장슬롯선택」「최고기록 달성에 실패했습니다.⏎」「최고기록:」「⏎현재기록:」; ← 38f98,39038 |
| `3c580` | 72 | 이벤트(스크립트 UI) | (fillRect(gfx), drawLine, slotComposite …) | ← 3c5c8,3c63c |
| `3c710` | 108 | 이벤트(스크립트 UI) | (fillRect(gfx), drawLine, slotComposite …) | ← 38e18 |
| `3c77c` | 118 | 이벤트(스크립트 UI) | (fillRect(gfx) …) | ← 3a4a4,3c7f4 |
| `3ccfc` | 176 | 이벤트(스크립트 UI) | (fillRect(gfx), drawText(xy), slotFrame …) | 「전투하시겠습니까?」「경품교환」「대화하기」「나가기」; ← 3a4a4 |
| `3cdc0` | 88 | 이벤트(스크립트 UI) | (fillRect(gfx), drawText(xy), slotFrame …) | 「슬롯머신」「코인구매」「경품교환」「나가기」; ← 3a4a4 |
| `3cf10` | 124 | 이벤트(스크립트 UI) | (fillRect(gfx), drawText(xy), slotFrame …) | 「도움말 선택」「나가기」; ← 3a4a4 |
| `3d018` | 104 | 이벤트(스크립트 UI) | (fillRect(gfx), drawText(xy), slotFrame …) | 「예」「아니오」「다시하기」; ← 3a4a4 |
| `108f4` | 190 | 전투 준비 | slotFrame×1 |  |
| `20878` | 22 | 전투 준비 | (fillRect(gfx), fillRect …) | ← 108f4 |
| `20890` | 84 | 전투 준비 | fillRect(gfx)×1 | ← 20878 |
| `ca2c` | 132 | 전투 | sprComposite×1 | ← 1c1ac,1e0d8 |
| `109f0` | 28 | 전투 | (slotAnim2, slotFrame, textBox(x,y,w,h) …) |  |
| `118cc` | 28 | 전투 | drawNumber(bitmap)×1 | ← 11e48,15724,1c1ac,31474 |
| `156c8` | 90 | 전투 | slotFrame×2 | ← 15724 |
| `15724` | 356 | 전투 | slotAnim2×1, slotFrame×1 | ← 1bcd4 |
| `18f7c` | 20 | 전투 | slotAnim×1 | vtable vt4d81c[10] |
| `18f90` | 20 | 전투 | slotAnim3×1 | vtable vt4d81c[11] |
| `18fa4` | 458 | 전투 | slotAnim×1 | vtable vt4d81c[12] |
| `196c0` | 46 | 전투 | slotFrame×1 | vtable vt4d81c[16] |
| `1b1d0` | 732 | 전투 | fillRect(gfx)×1 | ← 108f4 |
| `1bcd4` | 1240 | 전투 | (slotAnim2, slotFrame, textBox(x,y,w,h) …) | ← 109f0 |
| `1c1ac` | 734 | 전투 | slot170×1, window×4, textBox(x,y,w,h)×2, slotFrame×3 | ← 1bcd4 |
| `1c4cc` | 372 | 전투 | (fillRect(gfx), slotAnim3 …) | ← 109f0 |
| `1cf38` | 356 | 전투 | fillRect(gfx)×1 | 「 【  LEVEL UP  】⏎ GP를 얻었습니다」; ← 1c4cc |
| `1d140` | 42 | 전투 | (slotAnim3, fillRect(gfx), slotAnim …) | ← 1c4cc |
| `1d2b4` | 24 | 전투 | (fillRect(gfx) …) | ← 1c4cc |
| `1d2cc` | 42 | 전투 | (fillRect(gfx) …) | ← 1c4cc |
| `1d650` | 126 | 전투 | slotFrame×2 | ← 1c1ac,1f3d0 |
| `1df28` | 132 | 전투 | drawRect×3, fillRect×1, window×1 | ← 1e0d8,1ee3c,1f3d0 |
| `1dfec` | 188 | 전투 | measure×1, window×1, textBox(char*)×1 | ← 1bcd4 |
| `1e0d8` | 990 | 전투 | gaugeBox×4, fillRect×3, fillRect(gfx)×1, slotFrame×2 | ← 1bcd4 |
| `1e894` | 526 | 전투 | fillRect×2, gaugeBox×1, slotFrame×1, slotFrameFx×1 | ← 1bcd4 |
| `1ec54` | 408 | 전투 | drawText(xy)×1, slotFrame×1 | ← 1e894 |
| `1ee3c` | 628 | 전투 | slotFrame×2, fillRect×2 | ← 1bcd4 |
| `1f3d0` | 1284 | 전투 | slotFrame×4, drawText(xy)×2, gaugeBox×3, textBox(x,y,w,h)×1 | 「(%d)」; vtable vt4d870[3] |
| `1fe38` | 596 | 전투 | drawText(xy)×1, slotFrame×1, gaugeBox×1, textBox(x,y,w,h)×1 | 「사용할 수 있는 필살기술이 없습니다」「사용할 수 있는 스킬이 없습니다.」; vtable vt4d880[3] |
| `21b84` | 270 | 전투 | (slot174 …) | ← 26c58,2e584,387c8 |
| `2839c` | 176 | 전투 | fillRect(gfx)×1 | ← 2b8a0,2bbe8 |
| `28660` | 98 | 전투 | slotAnim×1, slotAnim3×1 | ← 2b8a0 |
| `286c8` | 118 | 전투 | slotAnim3×1 | ← 2b8a0 |
| `2b8a0` | 446 | 전투 | slotAnim3×1 | ← 1d140 |
| `2bbe8` | 154 | 전투 | (fillRect(gfx) …) | ← 1d2b4,1d2cc |
| `2dc2c` | 252 | 전투 | slot174×1 | ← 21b84 |
| `2e0a8` | 88 | 전투 | fillRect×1 | ← 3892c |
| `37b2c` | 216 | 전투 | slotFrameFx×1 | ← 1e0d8,1ee3c |
| `387c8` | 356 | 전투 | (slot174 …) | ← 1b1d0 |
| `3892c` | 428 | 전투 | fillRect(gfx)×1 | ← 1bcd4,2839c |
| `10af0` | 28 | 미니게임 | (sprComposite, slot170, drawText(xy) …) |  |
| `10b0c` | 28 | 미니게임 | (sprComposite, slot170, drawText(xy) …) |  |
| `26db8` | 122 | 미니게임 | (sprComposite, slot170, drawText(xy) …) | ← 10af0,10b0c |
| `26e44` | 64 | 미니게임 | (drawText(xy), sprComposite, drawNumber(bitmap) …) | ← 26db8 |
| `26e84` | 92 | 미니게임 | (drawText(xy), sprComposite, drawNumber(bitmap) …) | ← 26db8 |
| `26ee0` | 50 | 미니게임 | (drawText(xy), sprComposite, drawNumber(bitmap) …) | ← 26db8 |
| `26f14` | 520 | 미니게임 | (drawText(xy), sprComposite, drawNumber(bitmap) …) | ← 26db8 |
| `27144` | 172 | 미니게임 | sprComposite×2 | ← 2757c |
| `271f8` | 164 | 미니게임 | (sprComposite, fillRect, drawRect …) | ← 26db8 |
| `272a4` | 132 | 미니게임 | (sprComposite, fillRect, drawRect …) | ← 26db8 |
| `27328` | 196 | 미니게임 | sprComposite×1 | ← 26db8 |
| `27400` | 200 | 미니게임 | slot170×1, sprComposite×1 | ← 26db8 |
| `274c8` | 172 | 미니게임 | sprComposite×2 | ← 2757c |
| `2757c` | 106 | 미니게임 | (sprComposite, fillRect(gfx), textBoxMain …) | ← 26db8 |
| `275f0` | 190 | 미니게임 | drawText(xy)×1 | 「끝내시겠습니까?」; ← 26db8 |
| `276b8` | 552 | 미니게임 | drawText(xy)×5, sprComposite×4 | 「x%4d」「Score」「준비~」「게임 끝」; ← 26e44,26e84,26ee0,26f14 |
| `27900` | 160 | 미니게임 | sprComposite×2, drawNumber(bitmap)×2 | ← 276b8 |
| `279a4` | 156 | 미니게임 | fillRect×3, gaugeBox×1, sprComposite×2 | ← 276b8 |
| `27a4c` | 348 | 미니게임 | fillRect×6, sprComposite×1 | ← 276b8 |
| `27bc8` | 38 | 미니게임 | (sprComposite, fillRect, drawRect …) | ← 271f8,272a4,27328,27400 |
| `27bf0` | 1084 | 미니게임 | fillRect×15, drawRect×3, textBoxMain×5, sprComposite×4, drawNumber(bitmap)×2 | 「腱$$」「$$」「COIN」「%d」; ← 27bc8 |
| `28030` | 192 | 미니게임 | fillRect(gfx)×1, window×2, textBoxMain×1 | ← 2757c,275f0,276b8,27bc8 |
| `28114` | 190 | 미니게임 | sprComposite×1 | ← 27bf0 |
| `b220` | 80 | 미확인 | (fillRect(gfx) …) | ← 62fc |
| `b354` | 736 | 미확인 | (fillRect(gfx) …) | ← b220 |
| `bba0` | 498 | 미확인 | (sprComposite, fillRect, drawRect …) | ← 6720,bba0 |
| `e318` | 116 | 미확인 | (drawText(xy), measure, fillRect(gfx) …) | ← 23cd4 |
| `e622` | 498 | 미확인 | fillRect(gfx)×1, sprAnimFx×3 |  |
| `12efc` | 192 | 미확인 | (fillRect(gfx), sprComposite, fillRect …) | ← 64f8 |
| `18f5c` | 32 | 미확인 | slotAnim×1 |  |
| `22478` | 134 | 미확인 | slot174×1 | vtable vt4d8c4[2] |
| `26c58` | 170 | 미확인(scene19) | (slot174 …) | ← 26b2c |
| `10b8c` | 840 | 미확인(scene21, 지도?) | window×2, textBox(x,y,w,h)×1, drawRect×2, gaugeBox×2 |  |
| `2f06c` | 248 | 미확인(scene21, 지도?) | fillRect×1 | ← 10b8c |
| `302b4` | 1472 | 미확인(scene21, 지도?) | window×1, sprComposite×52, textBoxMain×3, sprFrame×1 | 「Help」「Exit」「저장」「이전」; ← 10b8c,27144,274c8,275f0 |
| `f748` | 232 | 이벤트(스크립트 UI) / 타이틀 메뉴 | textBox(char*)×1 | ← f8c0,fc9c,fe68,ffbc |
| `134cc` | 1586 | 이벤트(스크립트 UI) / 타이틀 메뉴 | fillRect×1 | 「에픽랭킹」「최고기록 달성.⏎내점수: %d점⏎랭킹에 등록할까요?」「1.Yes      2.No」「211.115.203.7」「내점」; ← ebc4,38e18,3a4a4 |
| `13b40` | 122 | 이벤트(스크립트 UI) / 타이틀 메뉴 | textBox(x,y,w,h)×2 | ← 134cc |
| `14274` | 92 | 이벤트(스크립트 UI) / 타이틀 메뉴 | fillRect(gfx)×1, drawRect×3 | ← f748,f8c0,fef0,100a0 |
| `cdf0` | 48 | 이벤트(스크립트 UI) / 필드 | (fillRect(gfx), slotAnim3, fillRect …) | ← c958,3a4a4 |
| `38338` | 170 | 이벤트(스크립트 UI) / 필드 | blitFx×1 | ← 38418 |
| `38418` | 850 | 이벤트(스크립트 UI) / 필드 | fillRect(gfx)×1 | ← cdf0 |
| `15618` | 128 | 전투 / 필드 | slotAnim3×1 | ← 17c00,18fa4 |
| `2e2ac` | 68 | 전투 / 필드 | (blit, blitV, blitH …) | ← 2f170,3892c |
| `2e2f8` | 232 | 전투 / 필드 | (blit, blitV, blitH …) | ← 2e100,2e2ac,2e2f8 |
| `2fffc` | 84 | 전투 / 필드 메뉴/미확인(4db90, 상태?) / 필드 메뉴/장비 | drawLine(gfx)×3 | ← 1e894,32338,32778,3312c |
| `30884` | 32 | 필드 메뉴/메인 / 필드 메뉴/미확인(4db90, 상태?) / 필드 메뉴/시스템(도움말·저장·옵션) / 필드 메뉴/장비 / 필드 메뉴/전투동료 / 필드 메뉴/퀘스트 | (sprComposite, sprFrame, textBoxMain …) | ← 310e8,31e74,32148,32338 |
| `300a0` | 196 | 필드 메뉴/미확인(4db90, 상태?) / 필드 메뉴/스킬 / 필드 메뉴/장비 | window×1, sprComposite×1 | 「斟俵漂迭曦溢諦砦寀」; ← 32778,3312c,33d94 |
| `30170` | 84 | 필드 메뉴/상점 / 필드 메뉴/아이템 | drawText(xy)×1, measure×1 | ← 31e74,32338,36250,36534 |
| `301f4` | 70 | 필드 메뉴/상점 / 필드 메뉴/아이템 | slotFrame×2 | ← 31cec,32148,32338,36250 |
| `30cac` | 222 | 필드 메뉴/아이템 / 필드 메뉴/장비 | window×1, gaugeBox×1, textBox(char*)×1 | ← 31b18,32338,32778,3403c |

## UI 문자열(바이너리 안, CP949)

데이터 구역 0x46878~0x4e624에서 뽑은 270개 가운데 UI와 관련된 것이다(JNI/WIPI 이름과 libc 문자열은 뺐다). 아이템, 스킬, 대사 문자열은 바이너리에 없다(dataset.dat, script.bin).

| 참조 함수 | 문자열(오프셋) |
|---|---|
| 15c0 | `46878` kt68wipiwicgs.magicn.com:27090 |
| bfe8 | `477b0` #:Fast; `477b8` *:Exit |
| e588 | `477c0` 프로그램오류!!! |
| ebc4 | `477d0` 게임시작; `477dc` 게임정보; `477f4` 네트워크; `47800` 게임종료; `4780c` 이어하기; `47818` 새게임; `47820` 트리플슬롯; `4782c` 저들의영웅; `47838` 만든사람들; `47870` 에픽 OST |
| ebc4,f8c0,359f8 | `477e8` 환경설정 |
| ebc4,13060 | `47844` 에픽한마디 |
| ebc4,fc9c | `47850` 게임문의 |
| ebc4,100a0,34f48 | `4785c` 도움말 |
| ebc4,100a0 | `47864` 게임속으로 |
| ebc4,fe68 | `4787c` 정보이용료 |
| ebc4,134cc | `47888` 에픽랭킹 |
| f8c0,302b4,355ac | `478a0` 저장 |
| f8c0,fef0,100a0,13060,302b4 | `478a8` 이전 |
| f8c0 | `478b0` 퀵모드; `478d4` 전투 |
| f8c0,359f8 | `478c4` 속도; `478cc` 사운드 |
| fe68 | `478e8` 네트워크사용량에 따라 기본통신료외 1패킷(512byte)당 0원의 추가정보 이용료가 부과됩니다. |
| ffbc | `476b4` fee le monde 히데오의 '여행을떠나며'. 에픽의 메인테마곡.; `476f0` Fel의 '풍림화산'. 이루아대륙의 테마곡.; `47718` TAM의 레퀴엠. 슬픔의 소곡으로 이오의 테마곡.; `47748` fee le monde 히데오의 'shinden'. 신전의 테마곡; `47778` fee le monde 히데오의 'tekikiti'. 던전, 모험의 테마곡 |
| 10410 | `47940` Loading..... |
| 10ef0 | `47950` 속도최적화...; `479c8` OK : 계속 |
| 10ef0,40bdc,43324 | `47960` 게임종료 후 재시작해야 합니다.⏎게임속도가 느릴경우, 메인메뉴 환경설정에서 퀵모드를 On으로 변경하세요. |
| 110d4,20c94,2166c | `479d4` %s%d.sav; `479e0` slot |
| 12b18 | `479e8` 【이착륙:0번키】 |
| 12c08 | `479fc` %d마리 |
| 13454,134cc | `47a04` epic1_pe |
| 134cc | `47a10` 최고기록 달성.⏎내점수: %d점⏎랭킹에 등록할까요?; `47a40` 1.Yes      2.No; `47a60` 내점수: %d점⏎랭킹을 볼까요?; `47a7c` 에픽랭킹은 엔딩 후 보스열전 점수로 계산됩니다. 랭킹을 볼까요?; `47abc` 서버접속중...; `47acc` CLR : 취소; `47ad8` 접속 오류가 발생했습니다. 잠시 후 다시 시도하세요.; `47b0c` CLR : 나가기; `47b1c` 서버응답 오류가 발생했습니다. 잠시 후 다시 시도하세요.; `47b54` 응답대기중...; `47b64` 내점수: %d점⏎; `47b74` 내순위: %d위⏎ |
| 14c68 | `47ba4` PHONENUMBER; `47bb0` PHONEMODEL |
| 14c68,40bdc,41e0c | `47bbc` SPH-V6900 |
| 177f8,1b898,1c8a8,1f25c | `489d8` MP 부족 |
| 178f8 | `489e0` MG 부족 |
| 1cf38 | `48a0c`  【  LEVEL UP  】⏎ GP를 얻었습니다 |
| 1e594 | `48a30` 도망 |
| 1fe38 | `48a98` 사용할 수 있는 필살기술이 없습니다; `48abc` 사용할 수 있는 스킬이 없습니다. |
| 20b04,20b98 | `48adc` gopt.sav |
| 20c94 | `48ae8` 에픽매니아 |
| 2171c,22b44 | `48af4` map.pmd |
| 21ac4 | `48afc` tileset.pts |
| 21c98 | `48b08` dataset.dat |
| 223a0 | `48b14` res.gmf |
| 228d4 | `48b1c` script.bin |
| 275f0 | `49224` 끝내시겠습니까? |
| 276b8 | `49234` x%4d; `4923c` Score; `49244` 준비~; `4924c` 게임 끝 |
| 27bf0 | `49254` COIN |
| 2b734 | `49bfa` 2KdK2 |
| 302b4 | `4ba58` Exit |
| 31e74 | `4bab4` 판매하시겠습니까?; `4bad4` 판매개수; `4bae0` 판매금액 |
| 31e74,367bc,37c08,397d0,3d018 | `4bac8` 예; `4bacc` 아니오 |
| 32778 | `4baec` 장착불가 Lv%d; `4bafc` 소지한 장비가 없습니다. |
| 338b8 | `4bb14` 퀘스트; `4bb1c` 등록된 퀘스트가 없습니다. |
| 33ef0 | `4bb40` 필 살 기 x; `4bb4c` 조합 스킬 x; `4bb58` 일반 스킬 x |
| 3403c | `4bb64` 3,6,9 단축키; `4bb74` 사용가능 |
| 34528 | `4bb80` MP부족 |
| 34cb8 | `4bb88` 전투동료설정 |
| 35470 | `4bb98` 종료하시겠습니까? |
| 355ac | `4bbac` 저장/불러오기; `4bbbc` 불러오기 |
| 356e8 | `4bbc8` Total time ; `4bbd4` Total.Time ; `4bc00` 저장 중!! 완료까지 종료하면 안됩니다.; `4bc74` 비어있음 |
| 359f8 | `478b8` On; `478bc` Off; `4bc80` 타이핑; `4bc88` 효과; `4bc90` 보통; `4bc98` 최상 |
| 35c1c | `4bca0` 옵션 |
| 360b0 | `4bca8` 아이템 구입; `4bcb4` 아이템 판매; `4bcc8` 무기류; `4bcd0` 방어구류 |
| 36250 | `4bce8` 가격 |
| 36534 | `4bcf0` %d개; `4bcf8` 코인: %dC |
| 367bc | `4bd04` 구매개수; `4bd28` 소지코인; `4bd60` 구입하시겠습니까? |
| 36a84 | `4bd84` 구매코인개수; `4bd94` 구매금액 %d |
| 36a84,3cdc0 | `4bda0` 코인구매 |
| 39840,39954 | `4c348`  골드 |
| 398d4 | `4bbe0` 저장하겠습니까? |
| 39914 | `4c350` 헌화하겠습니까? |
| 39954 | `4c360` 꽃  |
| 3a4a4 | `49174` 거인의 사과 2개 획득; `4918c` 제레미 스킬 생츄어리 획득; `4c370` 파티 GP ; `4c38c` 저장슬롯선택; `4c3bc` 최고기록 달성에 실패했습니다.⏎; `4c3dc` 최고기록:; `4c3e8` ⏎현재기록: |
| 3ccfc,3cdc0,3cf10 | `4bcc0` 나가기 |
| 3ccfc | `4c3f4` 전투하시겠습니까?; `4c414` 대화하기 |
| 3ccfc,3cdc0 | `4c408` 경품교환 |
| 3cdc0 | `4c420` 슬롯머신 |
| 3cf10 | `4c42c` 도움말 선택 |
| 3d018 | `4c44c` 다시하기 |
| 40370 | `4d454` fmod |
| 40bdc | `4d634` 0123456789abcdef; `4d648` (null) |
| 44a04 | `4d7a0` _sbrk: Heap and stack collision⏎ |

직접 참조가 안 잡힌 한글 문자열(같은 묶음의 앞 문자열 기준 + 상수 오프셋으로 접근하는 것으로 보임):

- `47684` 월드테마 (앞 참조 `46878`←15c0)
- `47690` 이오의테마 (앞 참조 `46878`←15c0)
- `4769c` 신전테마 (앞 참조 `46878`←15c0)
- `476a8` 모험의테마 (앞 참조 `46878`←15c0)
- `478dc` 턴 (앞 참조 `478d4`←f8c0)
- `478e0` 액티브 (앞 참조 `478d4`←f8c0)
- `47b84` %d위 : %u점⏎ (앞 참조 `47b74`←134cc)
- `47b98` 작업취소... (앞 참조 `47b74`←134cc)
- `489e8` 도망 실패 (앞 참조 `489e0`←178f8)
- `489f4` 도망 성공 (앞 참조 `489e0`←178f8)
- `48a00` %s스킬획득 (앞 참조 `489e0`←178f8)
- `48a40` 소지한 아이템이 없습니다 (앞 참조 `48a30`←1e594)
- `48a5c` 핫슬롯 아이템 미등록 (앞 참조 `48a30`←1e594)
- `48a74` 사용할 수 있는 조합기술이 없습니다. (앞 참조 `48a30`←1e594)
- `491a8` 소울워터 5개 획득 (앞 참조 `4918c`←3a4a4)
- `491bc` 조합기 파이널퓨전 획득 (앞 참조 `4918c`←3a4a4)
- `491d4` 미라클 포션 획득 (앞 참조 `4918c`←3a4a4)
- `491e8` 도신의 카드 획득 (앞 참조 `4918c`←3a4a4)
- `491fc` 도신의 동전 획득 (앞 참조 `4918c`←3a4a4)
- `49210` 도신의 주사위 획득 (앞 참조 `4918c`←3a4a4)
- `49268` 저 들의 영웅 (앞 참조 `49254`←27bf0)
- `49278` 벼베기슬레이어 (앞 참조 `49254`←27bf0)
- `49288` 벼베기전 외전 (앞 참조 `49254`←27bf0)
- `49298` 벼베기무한대전 (앞 참조 `49254`←27bf0)
- `492a8` 벼베기 타이쿤 (앞 참조 `49254`←27bf0)
- `492b8` 모두의 벼베기 (앞 참조 `49254`←27bf0)
- `492c8` 트리플 슬롯 (앞 참조 `49254`←27bf0)
- `4ada2` 장비물품 (앞 참조 `49bfa`←2b734)
- `4adac` 장신구류 (앞 참조 `49bfa`←2b734)
- `4adb6` 키아이템 (앞 참조 `49bfa`←2b734)
- `4af08` 장갑/방패류 (앞 참조 `49bfa`←2b734)
- `4af14` 갑옷류 (앞 참조 `49bfa`←2b734)
- `4af20` 투구류 (앞 참조 `49bfa`←2b734)
- `4af2c` 악세사리류 (앞 참조 `49bfa`←2b734)
- `4b51d` 장착 (앞 참조 `49bfa`←2b734)
- `4b522` 정보 (앞 참조 `49bfa`←2b734)
- `4b8bc` 환경설정 (앞 참조 `49bfa`←2b734)
- `4b8cc` 도움말 (앞 참조 `49bfa`←2b734)
- `4b8dc` 게임종료 (앞 참조 `49bfa`←2b734)
- `4ba60` 사용불가 (앞 참조 `4ba58`←302b4)
- `4ba6c` 판매불가 (앞 참조 `4ba58`←302b4)
- `4ba78` 핫슬롯 저장 (앞 참조 `4ba58`←302b4)
- `4ba84` 아이템 중복 (앞 참조 `4ba58`←302b4)
- `4bbf0` 불러오겠습니까? (앞 참조 `4bbe0`←398d4)
- `4bc28` 저장하였습니다. (앞 참조 `4bc00`←356e8)
- `4bc38` 게임 초기화 실패! (앞 참조 `4bc00`←356e8)
- `4bc4c` 저장공간 부족!!! (앞 참조 `4bc00`←356e8)
- `4bc60` 파일 버전 틀림!!! (앞 참조 `4bc00`←356e8)
- `4bcdc` 액세사리류 (앞 참조 `4bcd0`←360b0)
- `4bd10` 구매금액 (앞 참조 `4bd04`←367bc)
- `4bd1c` 구매코인 (앞 참조 `4bd04`←367bc)
- `4bd34` 소지금 부족! (앞 참조 `4bd28`←367bc)
- `4bd44` 코인 부족! (앞 참조 `4bd28`←367bc)
- `4bd50` 코인 MAX 초과! (앞 참조 `4bd28`←367bc)
- `4bd74` 수량 MAX 초과! (앞 참조 `4bd60`←367bc)
- `4c364`  골드 획득 (앞 참조 `4c360`←39954)
- `4c3b0` %d.빈 슬롯 (앞 참조 `4c38c`←3a4a4)
- `4c438` 레버를 내릴까요? (앞 참조 `4c42c`←3cf10)
- `4c458` 레버를 올릴까요? (앞 참조 `4c44c`←3d018)
- `4c765` 뉽Clet$CletCard;+cvs (앞 참조 `4c44c`←3d018)
- `4c7d2` 꿑org/kwis/msp/lcdui/Display;+display (앞 참조 `4c44c`←3d018)
- `4c7f8` 픇+KEY_PRESSED (앞 참조 `4c44c`←3d018)
- `4d6a9` 댠@ (앞 참조 `4d648`←40bdc)
- `4d6c8` 꾒뾃 (앞 참조 `4d648`←40bdc)
- `4d70d` 졬꿞4vC (앞 참조 `4d648`←40bdc)
