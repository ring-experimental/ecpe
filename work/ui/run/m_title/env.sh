# 타이틀 영역 시험 도우미. source 해서 쓴다.
source ${${(%):-%x}:A:h}/../../../config.sh
ROOT=$ECPE_ROOT
export ECPE_HEADLESS=1 ECPE_BIN=$ROOT/native/build/ecpe ECPE_UI_DIR=$ROOT/native/ui
N=$ROOT/work/tools/nctl.sh
RUN=$ROOT/work/ui/run/m_title
# fresh <dir> <width>: 저장 없이 띄운다
fresh() { $N stop $1 2>/dev/null; rm -rf $1; mkdir -p $1; $N start $1 --width $2 --scale 3; }
# saved <dir> <width>: 사용자 저장 사본으로 띄운다
saved() { $N stop $1 2>/dev/null; rm -rf $1; mkdir -p $1; cp -R "$ECPE_USER_SAVE" $1/save; $N start $1 --width $2 --scale 3; }
# pair <dir> <name>: 새 UI와 원작 그리기 둘 다 찍는다
pair() { $N $1 wait0.3 wshot $2 wait0.3 classic wait0.3 wshot ${2}_c wait0.3 modern wait0.2 >/dev/null; }
T=$ECPE_ROOT/work/ui/run/scripts/session/ttl
sheet() { local o=$1; shift; $ECPE_PY $T/sheet.py $o "$@"; }
# tomenu <dir>: 최적화가 끝난 저장으로 띄운 뒤 로고를 건너뛰어 타이틀 → 메인 메뉴
tomenu() { perl -e 'select(undef,undef,undef,2.5)'; $N $1 ok wait0.8 ok wait1.8 >/dev/null; }
# lv <dir> <레벨> <커서>: 메뉴 레벨을 바로 바꾼다(W=0x42bb6c, +0x164 레벨, +0x15c 커서)
lv() { $N $1 poke32 0x42bcd0 $2 poke32 0x42bcc8 $3 wait0.3 >/dev/null; }
# walk <dir> <접두>: 메인 메뉴에서 모든 레벨을 찍는다(해금 플래그를 켠다)
walk() {
  $N $1 poke32 0x42bcb8 0x5f022c wait0.2 >/dev/null
  lv $1 0 1; pair $1 ${2}l0
  lv $1 1 4; pair $1 ${2}l1
  lv $1 2 2; pair $1 ${2}l2
  lv $1 3 1; pair $1 ${2}l3
  lv $1 4 1; pair $1 ${2}l4
  lv $1 5 0; pair $1 ${2}l5
  lv $1 6 1; pair $1 ${2}l6
  lv $1 7 0; pair $1 ${2}l7
  lv $1 8 3; pair $1 ${2}l8
  lv $1 9 2; pair $1 ${2}l9
  lv $1 10 0; pair $1 ${2}l10
  lv $1 12 2; pair $1 ${2}l12
  lv $1 0 0
}
