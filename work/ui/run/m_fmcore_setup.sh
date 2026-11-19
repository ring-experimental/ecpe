#!/bin/zsh
# fmcore 시험용: 사본 저장으로 필드에 선 뒤 파티 5명, 긴 아이템 목록을 만든다(게스트 주소는 이 저장에서 늘 같다: 파티 0x42d5fc).
source ${0:A:h}/../../config.sh
source $ECPE_ROOT/work/ui/run/m_fmcore.env
W=${1:-568}
$ECPE_ROOT/work/tools/tofield.sh $R --width $W --scale 3
args=(poke32 0x42c5ec 0x8000 wait0.03 poke32 0x42c660 0 wait0.03 poke32 0x42d684 0x42c3f0 wait0.03 poke32 0x42d688 0x42c594 wait0.03 poke32 0x42d68c 0x42c738 wait0.03 poke32 0x42d690 0x42c8dc wait0.03)
CNT=0x42d6f4   # 파티 + 0xf8 = 보유 수
for id n in 11 1  1 5  2 3  3 1  4 9  5 2  6 1  7 1  8 2  9 1  12 4  13 2  14 2  15 1  16 1  18 3  19 1  20 1  22 2 \
            42 1  44 1  45 1  47 1  49 1  50 1  100 1 109 2 120 1 121 1 154 1 144 1 \
            164 1 165 1 170 1 173 1 \
            200 1 201 1 202 1 203 1 204 1 205 1 206 1 207 1 208 1 209 1 210 1 211 1 212 1 232 1; do
  args+=(poke8 $(printf "0x%x" $((CNT + id))) $n wait0.03)
done
$N $R $args wait0.3 >/dev/null
