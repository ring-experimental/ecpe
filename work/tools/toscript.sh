#!/bin/zsh
# toscript.sh <dir> <스크립트 번호> <시작 파일 오프셋(hex)> [ecpe 인자...]: 사용자 저장 사본으로 필드에 선 뒤 script.bin의 그 항목을 게스트 메모리에 올려
# 그 자리부터 실행시킨다(이벤트 장면 바로 보기). 풀린 스크립트는 <dir>/../scr/NN.bin을 쓴다(없으면 work/ui/run/fieldop/tools/unscr.py로 만든다).
# 자리는 보통 장면의 맵 이동 묶음([0xc1/0x3e] → 0x49) 첫 명령. 예: 불꽃놀이 5 228, 노을(니하트) 6 d88, 노을(이스나) 14 169c
source ${0:A:h}/../config.sh
ROOT=${0:A:h:h:h}; T=$ROOT/work/tools; D=${1:A}; N=$2; OFF=$3; shift 3
PY=$ECPE_PY; SCR=$ROOT/work/shots/scr
[[ -s $SCR/02.bin ]] || { mkdir -p $SCR; (cd $SCR:h && $PY $ROOT/work/ui/run/fieldop/tools/unscr.py >/dev/null) }
F=$SCR/$(printf %02d $N).bin
HDR=$($PY -c "import struct;d=open('$ROOT/work/game/script.bin','rb').read();print(hex(struct.unpack_from('<I',d,0xc+$N*12+8)[0]))")
STR=$(printf '0x%x' $((0x3f00100 + (HDR & 0xffff) * 4 + 4)))
$T/tofield.sh $D "$@"
# 바탕 +4의 u16은 코드 길이(워드, 0x39a6c가 넘는지 본다)라 항목 표의 셋째 워드를 그대로 적는다.
# VM 0x42d1e4: +0x10 바탕(파일 시작 - 8), +0x14 문자열, +0x2c 실행 자리, +0x25 상태 2(실행). G 0x42bb6c +0xc 장면 10(스크립트)
$T/nctl.sh $D raw "loadbin 3f00108 $F" poke32 0x3f00104 $HDR poke32 0x42d1f4 0x3f00100 poke32 0x42d1f8 $STR poke32 0x42d210 0x$OFF poke8 0x42d209 2 poke32 0x42bb78 10
