#!/bin/zsh
# 랭킹·스태프롤·오류 화면 찍기(폭별). 랭킹 하위 2는 실제 키(1=예)로 간다(포크로 2를 넣으면 접속 핸들이 없어 원본이 죽는다).
source ${0:A:h}/env.sh
OUT=$ROOT/work/ui/modern/title
for w in 568 240 640; do
  D=$RUN/final$w; rm -rf $D; mkdir -p $D; cp -R $RUN/f568/save $D/
  $N start $D --width $w --scale 3; tomenu $D; $N $D ok wait0.4 >/dev/null
  $N $D poke32 0x42bcb8 0x5f022c wait0.2 >/dev/null
  lv $D 4 1; $N $D ok wait0.5 >/dev/null; pair $D l11_r0
  $N $D poke32 0x42c018 12345 wait0.3 >/dev/null; pair $D l11_r0_score
  $N $D poke32 0x42c018 0 wait0.2 1 wait0.6 >/dev/null; pair $D l11_r2_error
  $N $D poke32 0x42bb7c 10 wait0.3 >/dev/null; pair $D l11_r10
  $N $D poke32 0x42bb7c 8 wait0.3 >/dev/null; pair $D l11_r8
  $N $D poke8 0x3f00000 65 poke32 0x3f0000c 99999 poke8 0x3f00014 65 poke32 0x3f00020 88888 poke8 0x3f00028 65 poke32 0x3f00034 77777 poke32 0x42c018 12345 poke32 0x42c01c 7 poke32 0x42c020 0x3f00000 poke32 0x42bb7c 7 wait0.3 >/dev/null; pair $D l11_r7_result
  $N $D poke32 0x42c020 0 poke32 0x42bb7c 9 wait0.3 clear wait0.5 >/dev/null
  lv $D 1 4; $N $D ok wait2.5 >/dev/null; pair $D staff
  $N $D star wait1.5 ok wait0.5 poke32 0x42bb78 0 wait0.3 >/dev/null; pair $D error
  $N $D ok wait0.4 >/dev/null; pair $D error_back
  echo "$w rml: $(grep -c 'rml [0-3]' $D/out.log) alive: $(ps -p $(cat $D/pid) >/dev/null && echo y || echo n)"
  $N stop $D; cp $D/*.png $OUT/w$w/; rm -f $OUT/w$w/now.png
done
