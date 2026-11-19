source ${${(%):-%x}:A:h}/../../../../config.sh
cd $ECPE_ROOT
export ECPE_HEADLESS=1 ECPE_BIN=$PWD/native/build/ecpe ECPE_UI_DIR=$PWD/native/ui
R=work/ui/run/m_fmchar
N=work/tools/nctl.sh
# 파티 5명 만들기(줄이 사라지지 않게 사이에 쉰다)
party5() { $N $R poke32 0x42d684 0x42c3f0 wait0.15 poke32 0x42d688 0x42c594 wait0.15 poke32 0x42d68c 0x42c738 wait0.15 poke32 0x42d690 0x42c8dc wait0.15 poke32 0x42d624 0x1f wait0.15 >/dev/null; }
# 아크에게 긴 일반 스킬 목록(15), 필살기 3, 조합 4를 준다
skills() { $N $R poke32 0x42c3ac 0x05030201 wait0.15 poke32 0x42c3b0 0x0d0c0706 wait0.15 poke32 0x42c3b4 0x1915130e wait0.15 poke32 0x42c3b8 0xff393837 wait0.15 poke32 0x42c3c0 15 wait0.15 poke32 0x42c3c4 0xff39370e wait0.15 poke32 0x42c3c8 3 wait0.15 poke8 0x42d6bc 0x60 wait0.15 poke8 0x42d6bd 0x18 wait0.15 >/dev/null; }
mem() { $N $R memdump $PWD/$R/f.bin >/dev/null; }
