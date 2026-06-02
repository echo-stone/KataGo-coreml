#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
CPP_DIR=$(cd "$SCRIPT_DIR/.." && pwd)
BUILD_DIR="$CPP_DIR/build"

MODEL_FILE=${KATAGO_MODEL:-"$BUILD_DIR/model.bin.gz"}
CONFIG_FILE=${KATAGO_CONFIG:-"$CPP_DIR/configs/misc/coreml_gtp.cfg"}
BENCH_THREADS=${KATAGO_BENCH_THREADS:-5}

cd "$BUILD_DIR"
./katago benchmark -model "$MODEL_FILE" -config "$CONFIG_FILE" -t "$BENCH_THREADS"
