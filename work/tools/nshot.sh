#!/bin/zsh
# nshot.sh <outdir> [ecpe 인자...] -- [keys...]: 네이티브 빌드를 임시 저장 폴더로 띄워 키를 보내고 키마다 화면을 ppm/png로 남긴다. waitN은 N초 대기.
OUT=${1:A}; shift; ARGS=()
while [[ $# -gt 0 && $1 != -- ]]; do ARGS+=$1; shift; done; shift
ROOT=${0:A:h:h:h}; BIN=$ROOT/native/build/ecpe
mkdir -p $OUT; CTL=$OUT/ctrl.txt; rm -f $CTL $OUT/s*.ppm(N) $OUT/s*.png(N)
[[ " $ARGS " == *" --save "* ]] || ARGS+=(--save $OUT/save)
$BIN $ROOT/native/build/game --ctrl $CTL $ARGS >$OUT/out.log 2>&1 &
P=$!; perl -e 'select(undef,undef,undef,2)'
n=0
for k in "$@"; do
  if [[ $k == wait* ]]; then perl -e "select(undef,undef,undef,${k#wait})"; continue; fi
  print "key $k" >> $CTL; perl -e 'select(undef,undef,undef,0.8)'
  n=$((n+1)); print "shot $OUT/s$n.ppm" >> $CTL; perl -e 'select(undef,undef,undef,0.3)'
done
print quit >> $CTL; perl -e 'select(undef,undef,undef,0.5)'; kill $P 2>/dev/null
for f in $OUT/s*.ppm(N); do sips -s format png $f --out ${f%.ppm}.png >/dev/null 2>&1; done
true
