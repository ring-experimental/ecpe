#!/bin/zsh
source ${0:A:h}/../../../config.sh
ROOT=$ECPE_ROOT; D=$ROOT/work/ui/run/enhance; N=$ROOT/work/tools/nctl.sh
$N $D key clear wait0.5 key down wait0.3 key down wait0.3 key ok wait0.4 key ok wait0.5
