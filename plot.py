#!/usr/bin/env python3
import argparse
import csv
import os
import matplotlib.pyplot as plt
import numpy as np

def read_tsv(filepath):
    """Читает TSV файл и возвращает список словарей."""
    rows = []
    with open(filepath, 'r', encoding='utf-8') as f:
        # Пропускаем строки комментариев (начинаются с #)
        lines = [line for line in f if not line.startswith('#')]
        
    if not lines:
        print(f"Файл {filepath} пуст или содержит только комментарии.")
        return rows

    reader = csv.DictReader(lines, delimiter='\t')
    for row in reader:
        rows.append(row)
    return rows

def parse_float(val):
    """Безопасно преобразует строку в float. Возвращает None для TIMEOUT."""
    if not val or val.strip().upper() == 'TIMEOUT':
        return None
    try:
        return float(val)
    except ValueError:
        return None

def plot_openmp(rows, out_dir):
    # Группируем по (N, k)
    groups = {}
    for r in rows:
        n = int(r['N'])
        k = int(r['k'])
        key = (n, k)
        if key not in groups:
            groups[key] = {'threads': [], 'par': [], 'seq': None}
        
        t = int(r['threads'])
        par = parse_float(r['parallel_time'])
        seq = parse_float(r['sequence_time'])
        
        groups[key]['threads'].append(t)
        groups[key]['par'].append(par)
        if seq is not None and groups[key]['seq'] is None:
            groups[key]['seq'] = seq

    for (n, k), data in groups.items():
        plt.figure(figsize=(8, 5))
        
        # Сортируем по числу потоков
        sorted_indices = np.argsort(data['threads'])
        threads = [data['threads'][i] for i in sorted_indices]
        par = [data['par'][i] for i in sorted_indices]
        
        # Фильтруем None (TIMEOUT)
        valid_t = [t for t, p in zip(threads, par) if p is not None]
        valid_p = [p for p in par if p is not None]
        
        plt.plot(valid_t, valid_p, marker='o', label='Параллельная (OpenMP)')
        if data['seq'] is not None:
            plt.axhline(y=data['seq'], color='r', linestyle='--', label='Последовательная')
            
        plt.title(f'OpenMP: N={n}, k={k}')
        plt.xlabel('Число потоков')
        plt.ylabel('Время (сек)')
        plt.grid(True)
        plt.legend()
        plt.xticks(valid_t)
        
        filename = os.path.join(out_dir, f'omp_N{n}_k{k}.png')
        plt.savefig(filename, dpi=300)
        plt.close()
        print(f"Сохранено: {filename}")

def plot_opencl(rows, out_dir):
    # Группируем по типу графа
    groups = {}
    for r in rows:
        typ = r['type']
        n = int(r['N'])
        if typ not in groups:
            groups[typ] = {'N': [], 'par': [], 'seq': []}
            
        groups[typ]['N'].append(n)
        groups[typ]['par'].append(parse_float(r['parallel_time']))
        groups[typ]['seq'].append(parse_float(r['sequence_time']))

    for typ, data in groups.items():
        plt.figure(figsize=(8, 5))
        
        sorted_indices = np.argsort(data['N'])
        Ns = [data['N'][i] for i in sorted_indices]
        par = [data['par'][i] for i in sorted_indices]
        seq = [data['seq'][i] for i in sorted_indices]
        
        # Заменяем None на NaN для matplotlib (чтобы линия прерывалась)
        par = [p if p is not None else np.nan for p in par]
        seq = [s if s is not None else np.nan for s in seq]
        
        plt.plot(Ns, par, marker='o', label='Параллельная (OpenCL)')
        plt.plot(Ns, seq, marker='s', label='Последовательная')
        
        plt.title(f'OpenCL: Тип графа "{typ}"')
        plt.xlabel('Размер матрицы (N)')
        plt.ylabel('Время (сек)')
        plt.yscale('log') # Логарифмическая шкала, т.к. разброс огромный
        plt.grid(True, which="both", ls="--")
        plt.legend()
        
        filename = os.path.join(out_dir, f'opencl_{typ}.png')
        plt.savefig(filename, dpi=300)
        plt.close()
        print(f"Сохранено: {filename}")

def plot_mpi(rows, out_dir):
    # Группируем по (M, N)
    groups = {}
    for r in rows:
        m = int(r['M'])
        n = int(r['N'])
        key = (m, n)
        if key not in groups:
            groups[key] = {'ranks': [], 'par': [], 'seq': None}
            
        ranks = int(r['Ranks'])
        par = parse_float(r['parallel_time'])
        seq = parse_float(r['sequence_time'])
        
        groups[key]['ranks'].append(ranks)
        groups[key]['par'].append(par)
        if seq is not None and groups[key]['seq'] is None:
            groups[key]['seq'] = seq

    for (m, n), data in groups.items():
        plt.figure(figsize=(8, 5))
        
        sorted_indices = np.argsort(data['ranks'])
        ranks = [data['ranks'][i] for i in sorted_indices]
        par = [data['par'][i] for i in sorted_indices]
        
        valid_r = [r for r, p in zip(ranks, par) if p is not None]
        valid_p = [p for p in par if p is not None]
        
        plt.plot(valid_r, valid_p, marker='o', label='Параллельная (MPI)')
        if data['seq'] is not None:
            plt.axhline(y=data['seq'], color='r', linestyle='--', label='Последовательная')
            
        plt.title(f'MPI: M={m}, N={n}')
        plt.xlabel('Число процессов')
        plt.ylabel('Время (сек)')
        plt.grid(True)
        plt.legend()
        plt.xticks(valid_r)
        
        filename = os.path.join(out_dir, f'mpi_M{m}_N{n}.png')
        plt.savefig(filename, dpi=300)
        plt.close()
        print(f"Сохранено: {filename}")

def plot_cuda(rows, out_dir):
    # Для CUDA все данные в одном файле (например, K=M=N)
    data = {'K': [], 'par': [], 'seq': []}
    for r in rows:
        k = int(r['K'])
        data['K'].append(k)
        data['par'].append(parse_float(r['parallel_time']))
        data['seq'].append(parse_float(r['sequence_time']))

    plt.figure(figsize=(8, 5))
    
    sorted_indices = np.argsort(data['K'])
    Ks = [data['K'][i] for i in sorted_indices]
    par = [data['par'][i] for i in sorted_indices]
    seq = [data['seq'][i] for i in sorted_indices]
    
    par = [p if p is not None else np.nan for p in par]
    seq = [s if s is not None else np.nan for s in seq]
    
    plt.plot(Ks, par, marker='o', label='Параллельная (CUDA)')
    plt.plot(Ks, seq, marker='s', label='Последовательная')
    
    plt.title('CUDA: Размер сетки K=M=N')
    plt.xlabel('Размер сетки (K)')
    plt.ylabel('Время (сек)')
    plt.yscale('log')
    plt.grid(True, which="both", ls="--")
    plt.legend()
    
    filename = os.path.join(out_dir, 'cuda.png')
    plt.savefig(filename, dpi=300)
    plt.close()
    print(f"Сохранено: {filename}")

def main():
    ap = argparse.ArgumentParser(description="Построение графиков по результатам бенчмарков.")
    ap.add_argument('--lab', required=True, choices=['openmp', 'opencl', 'mpi', 'cuda'],
                    help="Тип лабораторной работы")
    ap.add_argument('--file', required=True, help="Путь к TSV файлу с результатами")
    ap.add_argument('--out-dir', default='graphs', help="Директория для сохранения графиков")
    args = ap.parse_args()

    if not os.path.isfile(args.file):
        print(f"Файл {args.file} не найден.")
        return

    if not os.path.isdir(args.out_dir):
        os.makedirs(args.out_dir)

    rows = read_tsv(args.file)
    if not rows:
        return

    print(f"Загружено {len(rows)} строк. Построение графиков...")

    if args.lab == 'openmp':
        plot_openmp(rows, args.out_dir)
    elif args.lab == 'opencl':
        plot_opencl(rows, args.out_dir)
    elif args.lab == 'mpi':
        plot_mpi(rows, args.out_dir)
    elif args.lab == 'cuda':
        plot_cuda(rows, args.out_dir)
        
    print("Готово!")

if __name__ == '__main__':
    main()