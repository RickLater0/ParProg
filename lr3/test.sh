#!/bin/bash
set -e

M=${1:-}
N=${2:-}
RANKS=${3:-0}

if [ -z "$M" ] || [ -z "$N" ]; then
    echo ">>> M и N:"
    read -r M N
fi

if [ "$RANKS" -eq 0 ]; then
    if command -v nproc >/dev/null 2>&1; then
        RANKS=$(nproc)
    else
        RANKS=4
    fi
fi

echo ">>> [$M, $N]  MPI-ranks: $RANKS"

MPIRUN_FLAGS=""
if [ "$(id -u)" -eq 0 ]; then
    MPIRUN_FLAGS="--allow-run-as-root"     # удобно в Docker/CI
fi

mpirun $MPIRUN_FLAGS -np "$RANKS" ./lab.elf "$M" "$N"