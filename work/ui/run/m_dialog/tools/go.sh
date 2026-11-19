#!/bin/zsh
# go.sh <inst> [width] [scale]: 내 빌드로 저장 사본을 띄워 필드(장면 7)에 선다. 실행 폴더는 work/ui/run/m_dialog/<inst>
source ${0:A:h}/../../../../config.sh
R=$ECPE_ROOT; D=$R/work/ui/run/m_dialog/$1; P=$ECPE_PY
export ECPE_HEADLESS=1 ECPE_BIN=${XB:-$R/native/build/ecpe} ECPE_UI_DIR=$R/native/ui
for try in 1 2 3; do
  $R/work/tools/tofield.sh $D --width ${2:-568} --scale ${3:-3} >/dev/null
  $R/work/tools/nctl.sh $D wait1 memdump $D/st.bin >/dev/null
  st=$($P $R/work/ui/run/m_dialog/tools/state.py $D/st.bin 2>/dev/null)
  [[ ${st%% *} == 7 ]] && break
  echo "go retry $try ($st)" >&2
done
