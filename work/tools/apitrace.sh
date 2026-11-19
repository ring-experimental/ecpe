#!/bin/zsh
# apitrace.sh <zip> <outdir> [keys...]: 세이브를 건드리지 않도록 임시 HOME에서 WIE를 띄워 WIPI 호출을 기록한다.
# 키 사이 대기는 waitN. 프레임은 outdir/sN.ppm, 호출 집계는 outdir/calls.txt("횟수 id 호출지").
source ${0:A:h}/../config.sh
ZIP=$1; OUT=$2; shift 2; mkdir -p $OUT/home; CTL=$OUT/ctrl.txt; rm -f $CTL
BIN=$WIE
{ HOME=$OUT/home RUST_LOG=wie_ktf::runtime::wipi_c=trace WIE_CTRL=$CTL $BIN --windowed $ZIP 2>&1 >/dev/null \
  | grep --line-buffered -a 'wipic call' | sed -E 's/.*wipic call (0x[0-9a-f]+) from (0x[0-9a-f]+).*/\1 \2/' > $OUT/raw.txt } &
sleep 8
n=0
for k in "$@"; do
  if [[ $k == wait* ]]; then sleep ${k#wait}; else print "key $k" > $CTL; sleep 1.5; n=$((n+1)); print "shot $OUT/s$n.ppm" > $CTL; sleep 0.3; fi
done
print quit > $CTL; sleep 2
pkill -f "$BIN --windowed" 2>/dev/null
wait
sort $OUT/raw.txt | uniq -c | sort -rn > $OUT/calls.txt
wc -l $OUT/raw.txt $OUT/calls.txt
