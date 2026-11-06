#!/usr/bin/env bash

# Остановка при ошибках
set -e

# Определение функций

usage() {
    echo "Usage: $0 <n> [iso|non_iso]"
    echo "  n       - matrix size (e.g. 5, 10, 12)"
    echo "  iso     - generate isomorphic graphs (default)"
    echo "  non_iso - generate non-isomorphic graphs"
    exit 1
}

main() {
    local n=0
    local mode="iso"
    local lab_exec="./lab.elf"

    if [ "$#" -lt 1 ]; then
        usage
    fi

    n="$1"
    if [ "$#" -ge 2 ]; then
        mode="$2"
    fi

    if [ ! -x "$lab_exec" ]; then
        echo "Error: $lab_exec not found or not executable. Build it first!"
        exit 1
    fi

    # Генерация матриц через Python
    local args
    args=$(python3 - "$n" "$mode" << 'EOF'
import sys
import random

def generate_graphs(n, mode):
    # Создаем случайный симметричный граф G1 (без петель)
    m1 = [[0] * n for _ in range(n)]
    for i in range(n):
        for j in range(i + 1, n):
            val = random.choice([0, 1])
            m1[i][j] = val
            m1[j][i] = val

    m2 = [[0] * n for _ in range(n)]

    if mode == "iso":
        # Перестановка вершин (перестановка P)
        perm = list(range(n))
        random.shuffle(perm)
        for i in range(n):
            for j in range(n):
                m2[i][j] = m1[perm[i]][perm[j]]
    else:
        # Неизоморфный граф: случайно инвертируем ребра
        for i in range(n):
            for j in range(i + 1, n):
                val = random.choice([0, 1])
                m2[i][j] = val
                m2[j][i] = val

    # Преобразуем матрицы в строки разделенные пробелами
    str1 = " ".join(str(val) for row in m1 for val in row)
    str2 = " ".join(str(val) for row in m2 for val in row)

    print(f'"{str1}" "{str2}"')

if __name__ == "__main__":
    n_val = int(sys.argv[1])
    mode_val = sys.argv[2]
    generate_graphs(n_val, mode_val)
EOF
    )

    echo "Running: $lab_exec $n $args"
    echo "----------------------------------------"
    
    # Запуск лабораторной с переданными строками-матрицами
    eval "$lab_exec $n $args"
}

# Вызов главной функции
main "$@"