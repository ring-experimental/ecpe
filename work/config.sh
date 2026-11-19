# 시험·분석 스크립트(work/tools, work/ui/run)의 공용 설정. 각 스크립트가 자기 위치에서 상대 경로로 읽는다:
#   source ${0:A:h}/../config.sh   (깊이는 스크립트마다 다르다)
# 경로는 모두 이 파일 위치(저장소)를 기준으로 정한다. 기계마다 다른 값은 work/config.local.sh(.gitignore, config.local.sh.example 참고)에
# 적거나 환경 변수로 넘긴다. 이 파일에는 특정 기계의 경로를 적지 않는다.

# 저장소 루트
export ECPE_ROOT=${ECPE_ROOT:-${${(%):-%x}:A:h:h}}
[[ -f $ECPE_ROOT/work/config.local.sh ]] && source $ECPE_ROOT/work/config.local.sh

# 파이썬: 저장소 루트의 pyproject.toml/uv.lock 환경(uv). 다른 인터프리터를 쓰려면 ECPE_PY에 실행 파일 경로를 준다.
export ECPE_PY=${ECPE_PY:-$ECPE_ROOT/work/tools/py}
# 대조 기준 에뮬레이터 WIE(https://github.com/dlunch/wie) 실행 파일. 기본은 PATH의 wie.
export WIE=${WIE:-wie}
# 네이티브 빌드
export ECPE_NATIVE=${ECPE_NATIVE:-$ECPE_ROOT/native}
# 사용자 세이브(시험은 언제나 이 폴더의 사본으로 한다). 기본은 SDL_GetPrefPath("ecpe", "save")의 자리.
if [[ -z ${ECPE_USER_SAVE:-} ]]; then
  case $OSTYPE in
    darwin*) ECPE_USER_SAVE="$HOME/Library/Application Support/ecpe/save" ;;
    *) ECPE_USER_SAVE="${XDG_DATA_HOME:-$HOME/.local/share}/ecpe/save" ;;
  esac
fi
export ECPE_USER_SAVE
