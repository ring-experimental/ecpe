#!/bin/zsh
# door.sh <inst> [poke32 words...]: 연회장 문 앞으로 옮겨 "연회장은 이곳이 아냐!" 대사(스크립트 대기 상태 7)를 띄운다.
# 대사가 떴는지 메모리로 확인하고 안 떴으면 다시 한다. 인자가 있으면 그 다음 명령 자리(0x452354~)를 덮는다.
source ${0:A:h}/../../../../config.sh
R=$ECPE_ROOT; D=$R/work/ui/run/m_dialog/$1; shift
N=$R/work/tools/nctl.sh; P=$ECPE_PY
for try in 1 2 3 4 5; do
  $N $D poke32 0x42c254 410 poke32 0x42c258 340 wait0.3 key up 600 wait2 memdump $D/st.bin >/dev/null
  st=$($P $R/work/ui/run/m_dialog/tools/state.py $D/st.bin)
  [[ $st == "10 7" ]] && break
  echo "door retry $try ($st)" >&2; $N $D clear wait0.5 clear wait0.5 >/dev/null
done
a=$((0x452354)); L=()
for w in "$@"; do L+=(poke32 $(printf 0x%x $a) $w); a=$((a+4)); done
[[ ${#L} -gt 0 ]] && $N $D "${L[@]}" wait0.2 >/dev/null
true
