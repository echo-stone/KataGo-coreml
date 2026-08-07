#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
CPP_DIR=$(cd "$SCRIPT_DIR/.." && pwd)
REPO_DIR=$(cd "$CPP_DIR/.." && pwd)

BUILD_DIR=${KATAGO_COREML_BUILD_DIR:-"$CPP_DIR/build-coreml"}
INSTALL_PATH=${KATAGO_COREML_INSTALL_PATH:-"$HOME/.katago/katago"}
MODEL_FILE=${KATAGO_MODEL:-"$HOME/.katago/model.bin.gz"}
CONFIG_FILE=${KATAGO_CONFIG:-"$HOME/.katago/coreml_gtp.cfg"}
GENERATOR=${KATAGO_CMAKE_GENERATOR:-Ninja}
JOBS=${KATAGO_BUILD_JOBS:-$(sysctl -n hw.ncpu 2>/dev/null || echo 8)}
DO_CLEAN=0
DO_INSTALL=0
DO_SMOKE_GTP=0

# 명령행 사용법을 출력한다. 매개변수와 반환값은 없다.
usage() {
  cat <<EOF
Usage: $(basename "$0") [options]

Build the macOS Metal/CoreML KataGo binary with CMake.

Options:
  --build-dir DIR     Build directory. Default: $BUILD_DIR
  --config FILE       GTP config for --smoke-gtp. Default: $CONFIG_FILE
  --clean             Remove the build directory before configuring.
  --generator NAME    CMake generator. Default: $GENERATOR
  --install           Install the built binary to --install-path.
  --install-path FILE Install destination. Default: $INSTALL_PATH
  --jobs N            Parallel build jobs. Default: $JOBS
  --model FILE        Model for --smoke-gtp. Default: $MODEL_FILE
  --smoke-gtp         Run a short GTP startup check after building.
  -h, --help          Show this help.

Environment overrides:
  KATAGO_COREML_BUILD_DIR
  KATAGO_COREML_INSTALL_PATH
  KATAGO_CMAKE_GENERATOR
  KATAGO_BUILD_JOBS
  KATAGO_CONFIG
  KATAGO_MODEL
EOF
}

# 오류 메시지를 표준 오류에 출력하고 종료한다. $1은 출력할 메시지이며 반환하지 않는다.
die() {
  echo "error: $1" >&2
  exit 1
}

# 명령이 설치되어 있는지 확인한다. $1은 명령 이름이며 반환값은 없다.
require_cmd() {
  command -v "$1" >/dev/null 2>&1 || die "missing required command '$1'"
}

# 명령행 인자를 전역 설정값으로 변환한다. 모든 스크립트 인자를 받고 반환값은 없다.
parse_args() {
  while [ "$#" -gt 0 ]; do
    case "$1" in
      --build-dir)
        [ "$#" -ge 2 ] || die "--build-dir requires a value"
        BUILD_DIR=$2
        shift 2
        ;;
      --clean)
        DO_CLEAN=1
        shift
        ;;
      --config)
        [ "$#" -ge 2 ] || die "--config requires a value"
        CONFIG_FILE=$2
        shift 2
        ;;
      --generator)
        [ "$#" -ge 2 ] || die "--generator requires a value"
        GENERATOR=$2
        shift 2
        ;;
      --install)
        DO_INSTALL=1
        shift
        ;;
      --install-path)
        [ "$#" -ge 2 ] || die "--install-path requires a value"
        INSTALL_PATH=$2
        shift 2
        ;;
      --jobs)
        [ "$#" -ge 2 ] || die "--jobs requires a value"
        JOBS=$2
        shift 2
        ;;
      --model)
        [ "$#" -ge 2 ] || die "--model requires a value"
        MODEL_FILE=$2
        shift 2
        ;;
      --smoke-gtp)
        DO_SMOKE_GTP=1
        shift
        ;;
      -h|--help)
        usage
        exit 0
        ;;
      *)
        die "unknown option '$1'"
        ;;
    esac
  done
}

# 공식 Metal 백엔드로 CMake 빌드 디렉터리를 구성한다. 매개변수와 반환값은 없다.
configure_metal() {
  sdkroot=${KATAGO_COREML_SDKROOT:-$(xcrun --show-sdk-path)}

  cmake -S "$CPP_DIR" -B "$BUILD_DIR" -G "$GENERATOR" \
    -DNO_GIT_REVISION=1 \
    -DBUILD_DISTRIBUTED=0 \
    -DUSE_BACKEND=METAL \
    -DCMAKE_OSX_SYSROOT="$sdkroot"
}

# 구성된 디렉터리에서 katago 대상을 빌드한다. 매개변수와 반환값은 없다.
build_metal() {
  cmake --build "$BUILD_DIR" --target katago -j "$JOBS"
}

# 빌드 디렉터리를 안전하게 정리한다. 매개변수와 반환값은 없다.
clean_build_dir() {
  case "$BUILD_DIR" in
    ""|/|"$REPO_DIR"|"$CPP_DIR")
      die "refusing to clean unsafe build directory: $BUILD_DIR"
      ;;
  esac
  [ ! -L "$BUILD_DIR" ] || die "refusing to clean symlink build directory: $BUILD_DIR"
  rm -rf -- "$BUILD_DIR"
}

# 빌드한 바이너리를 설치한다. $1은 바이너리 경로이며 반환값은 없다.
install_binary() {
  binary=$1
  mkdir -p "$(dirname "$INSTALL_PATH")"
  install -m 755 "$binary" "$INSTALL_PATH"
  "$INSTALL_PATH" version
}

# 모델과 설정으로 짧은 GTP 시작 검사를 수행한다. $1은 바이너리 경로이며 반환값은 없다.
smoke_gtp() {
  binary=$1
  [ -f "$MODEL_FILE" ] || die "model file not found: $MODEL_FILE"
  [ -f "$CONFIG_FILE" ] || die "config file not found: $CONFIG_FILE"

  model_dir=$(cd "$(dirname "$MODEL_FILE")" && pwd)
  model_abs=$model_dir/$(basename "$MODEL_FILE")
  config_dir=$(cd "$(dirname "$CONFIG_FILE")" && pwd)
  config_abs=$config_dir/$(basename "$CONFIG_FILE")

  (cd "$model_dir" && printf 'quit\n' | "$binary" gtp -config "$config_abs" -model "$model_abs")
}

parse_args "$@"

case "$BUILD_DIR" in
  /*) ;;
  *) BUILD_DIR=$REPO_DIR/$BUILD_DIR ;;
esac

require_cmd cmake
require_cmd pkg-config
require_cmd xcrun
case "$GENERATOR" in
  Ninja) require_cmd ninja ;;
esac

if [ "$DO_CLEAN" -eq 1 ]; then
  clean_build_dir
fi

configure_metal
build_metal

BINARY=$BUILD_DIR/katago
[ -x "$BINARY" ] || die "built binary not found: $BINARY"
BINARY=$(cd "$(dirname "$BINARY")" && pwd)/$(basename "$BINARY")
"$BINARY" version

if [ "$DO_INSTALL" -eq 1 ]; then
  install_binary "$BINARY"
fi

if [ "$DO_SMOKE_GTP" -eq 1 ]; then
  smoke_gtp "$BINARY"
fi

echo "Metal/CoreML binary ready: $BINARY"
