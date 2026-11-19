#!/bin/zsh
# shot.sh <zip> <outdir> [--game-width N] -- [keys...]: WIE로 띄워 키를 보내고 키마다 화면을 ppm으로 남긴다. waitN은 N초 대기.
source ${0:A:h}/../config.sh
ZIP=${1:A}; OUT=${2:A}; shift 2; ARGS=()
while [[ $# -gt 0 && $1 != -- ]]; do ARGS+=$1; shift; done; shift
mkdir -p $OUT; CTL=$OUT/ctrl.txt; rm -f $CTL $OUT/s*.ppm(N)
WIE_CTRL=$CTL $WIE $ARGS $ZIP >/dev/null 2>$OUT/err.log &
P=$!; perl -e 'select(undef,undef,undef,6)'
n=0
for k in "$@"; do
  if [[ $k == wait* ]]; then perl -e "select(undef,undef,undef,${k#wait})"; continue; fi
  print "key $k" > $CTL; perl -e 'select(undef,undef,undef,1.2)'
  n=$((n+1)); print "shot $OUT/s$n.ppm" > $CTL; perl -e 'select(undef,undef,undef,0.4)'
done
print quit > $CTL; perl -e 'select(undef,undef,undef,1)'; kill $P 2>/dev/null; true
