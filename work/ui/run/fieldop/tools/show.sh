#!/bin/zsh
# show.sh <calltree> <fn> : subtree, filtered to non-leaf noise
source ${0:A:h}/../../../../config.sh
$ECPE_PY $ECPE_ROOT/work/ui/run/scripts/session/sub.py $1 $2 | grep -v "^ *\(4[0-9a-f]\{4\}\|144b0\|4284\|4474\|4400\|439c\|40324\|45f4\|39e4\|3298\|37e0\|6[0-9a-f]\{3\}\|7[0-9a-f]\{3\}\)\( x[0-9]*\)\?$" | grep -v "PutPixel\|GetStringWidth\|@ 4714\|@ 467c\|@ 45b8\|@ 3ecc" | sed 's/ lr=/ lr=/'
