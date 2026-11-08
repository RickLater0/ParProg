#!/bin/bash
set -e

K=${1:-128}
M=${2:-128}
N=${3:-128}
T1=${4:-100.0}
T2=${5:-0.0}
RADIUS=${6:-20}
MAX_ITER=${7:-1000}

echo ">>> grid ${K}x${M}x${N}, T1=${T1}, T2=${T2}, radius=${RADIUS}, max_iter=${MAX_ITER}"

./lab.elf "$K" "$M" "$N" "$T1" "$T2" "$RADIUS" "$MAX_ITER"