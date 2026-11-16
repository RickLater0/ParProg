#!/usr/bin/env python3
"""
Универсальный бенчмарк. Совместим с Python 3.6+.
Использует динамический таймер для последовательной части.
"""
import argparse
import os
import re
import statistics
import subprocess
import sys
import time
import selectors
import signal

TIME_RE = re.compile(r'Time:\s*([0-9.]+)\s*secs')
SEQ_RE = re.compile(r'\[seq\].*?([0-9.]+)\s*s')
PAR_RE = re.compile(r'\[par\].*?([0-9.]+)\s*s')

def parse_range(s):
    if ':' in s:
        parts = s.split(':')
        if len(parts) != 3:
            raise argparse.ArgumentTypeError("Invalid range: {}. Expected start:end:step".format(s))
        try:
            start, end, step = (int(x) for x in parts)
        except ValueError:
            raise argparse.ArgumentTypeError("Invalid range: {}".format(s))
        if step <= 0:
            raise argparse.ArgumentTypeError("Step must be positive: {}".format(s))
        return list(range(start, end + 1, step))
    try:
        return [int(x) for x in s.split(',')]
    except ValueError:
        raise argparse.ArgumentTypeError("Invalid list: {}".format(s))

def run_dynamic(cmd, env_extra, cwd, timeout_par, timeout_seq):
    env = os.environ.copy()
    env.update(env_extra)
    
    try:
        proc = subprocess.Popen(
            cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
            universal_newlines=True, env=env, cwd=cwd,
            start_new_session=True, bufsize=1
        )
    except Exception:
        return None, [], False

    sel = selectors.DefaultSelector()
    sel.register(proc.stdout, selectors.EVENT_READ)
    
    start_time = time.time()
    current_timeout = timeout_par
    output_lines = []
    timed_out = False
    
    while True:
        if time.time() - start_time > current_timeout:
            try:
                os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
            except ProcessLookupError:
                pass
            proc.wait()
            timed_out = True
            break
            
        events = sel.select(timeout=0.05)
        for key, _ in events:
            line = proc.stdout.readline()
            if not line:
                proc.wait()
                return proc.returncode, output_lines, timed_out
            
            output_lines.append(line)
            
            if "[par]" in line or "Parallel:" in line:
                start_time = time.time()
                current_timeout = timeout_seq
                
    return proc.returncode, output_lines, timed_out

def build(lab_dir, cwd):
    print("# Building {}...".format(lab_dir), flush=True)
    try:
        subprocess.run(
            ["make", "clean", "all"], cwd=cwd,
            check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
            universal_newlines=True,
        )
    except subprocess.CalledProcessError as e:
        print("# Build failed:", file=sys.stderr)
        print(e.stdout, file=sys.stderr)
        print(e.stderr, file=sys.stderr)
        raise

def cleanup(lab_dir, cwd):
    print("# Cleaning {}...".format(lab_dir), flush=True)
    try:
        subprocess.run(
            ["make", "clean"], cwd=cwd,
            check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
            universal_newlines=True,
        )
    except Exception:
        pass

def bench_openmp(args, lab_dir):
    cwd = os.path.join(os.getcwd(), lab_dir)
    binary = os.path.join(cwd, args.binary)
    print("N\tk\tthreads\tparallel_time\tsequence_time", flush=True)
    
    for N in args.N:
        for k in args.k:
            for p in args.p:
                par, seq = [], []
                for _ in range(args.runs):
                    cmd = ["stdbuf", "-oL", binary, str(N), str(k)]
                    ret, out_lines, timed_out = run_dynamic(
                        cmd, {'OMP_NUM_THREADS': str(p)},
                        cwd, args.timeout_par, args.timeout_seq
                    )
                    out = "".join(out_lines)
                    label = None
                    for line in out.splitlines():
                        s = line.strip()
                        if s == 'Parallel:': label = 'parallel'
                        elif s == 'Sequence:': label = 'sequence'
                        else:
                            m = TIME_RE.search(line)
                            if m and label:
                                if label == 'parallel': par.append(float(m.group(1)))
                                elif label == 'sequence': seq.append(float(m.group(1)))
                                label = None
                                
                pv = "{:.6f}".format(statistics.mean(par)) if par else "TIMEOUT"
                sv = "{:.6f}".format(statistics.mean(seq)) if seq else "TIMEOUT"
                print("{}\t{}\t{}\t{}\t{}".format(N, k, p, pv, sv), flush=True)

def bench_opencl(args, lab_dir):
    cwd = os.path.join(os.getcwd(), lab_dir)
    binary = os.path.join(cwd, args.binary)
    test_sh = os.path.join(cwd, "test.sh")
    
    if not os.path.isfile(test_sh):
        print("# test.sh not found in {}".format(lab_dir), file=sys.stderr)
        return

    print("N\ttype\tparallel_time\tsequence_time", flush=True)
    
    for N in args.N:
        for typ in args.type:
            if typ == 'reg' and N % 2 != 0:
                continue
            
            par, seq = [], []
            for _ in range(args.runs):
                cmd = ["stdbuf", "-oL", "bash", "test.sh", str(N), typ, binary]
                ret, out_lines, timed_out = run_dynamic(
                    cmd, {}, cwd, args.timeout_par, args.timeout_seq
                )
                out = "".join(out_lines)
                sm = SEQ_RE.search(out)
                pm = PAR_RE.search(out)
                
                if pm: par.append(float(pm.group(1)))
                if sm: seq.append(float(sm.group(1)))
                
            pv = "{:.6f}".format(statistics.mean(par)) if par else "TIMEOUT"
            sv = "{:.6f}".format(statistics.mean(seq)) if seq else "TIMEOUT"
            print("{}\t{}\t{}\t{}".format(N, typ, pv, sv), flush=True)

def bench_mpi(args, lab_dir):
    cwd = os.path.join(os.getcwd(), lab_dir)
    print("M\tN\tRanks\tparallel_time\tsequence_time", flush=True)
    
    for M in args.N:
        for N in args.k:
            for p in args.p:
                par, seq = [], []
                for _ in range(args.runs):
                    cmd = ["stdbuf", "-oL", "bash", "test.sh", str(M), str(N), str(p)]
                    ret, out_lines, timed_out = run_dynamic(
                        cmd, {}, cwd, args.timeout_par, args.timeout_seq
                    )
                    if ret is not None and ret != 0 and not timed_out:
                        print("# MPI crashed with code {}".format(ret), file=sys.stderr)
                    
                    out = "".join(out_lines)
                    sm = SEQ_RE.search(out)
                    pm = PAR_RE.search(out)
                    if pm: par.append(float(pm.group(1)))
                    if sm: seq.append(float(sm.group(1)))
                    
                pv = "{:.6f}".format(statistics.mean(par)) if par else "TIMEOUT"
                sv = "{:.6f}".format(statistics.mean(seq)) if seq else "TIMEOUT"
                print("{}\t{}\t{}\t{}\t{}".format(M, N, p, pv, sv), flush=True)

def bench_cuda(args, lab_dir):
    cwd = os.path.join(os.getcwd(), lab_dir)
    test_sh = os.path.join(cwd, "test.sh")
    
    if not os.path.isfile(test_sh):
        print("# test.sh not found in {}".format(lab_dir), file=sys.stderr)
        return

    # Для CUDA в test.sh: K M N T1 T2 RADIUS MAX_ITER
    # args.N - это K, args.k - это M. N зафиксируем равным K.
    print("K\tM\tN\titer\tparallel_time\tsequence_time", flush=True)
    
    for K in args.N:
        for M in args.k:
            N = K
            # Используем --extra для T1 T2 RADIUS MAX_ITER
            extra_args = args.extra if args.extra else ["100.0", "0.0", "10", "100"]
            
            par, seq = [], []
            for _ in range(args.runs):
                cmd = ["stdbuf", "-oL", "bash", "test.sh", str(K), str(M), str(N)] + extra_args
                ret, out_lines, timed_out = run_dynamic(
                    cmd, {}, cwd, args.timeout_par, args.timeout_seq
                )
                out = "".join(out_lines)
                sm = SEQ_RE.search(out)
                pm = PAR_RE.search(out)
                
                if pm: par.append(float(pm.group(1)))
                if sm: seq.append(float(sm.group(1)))
                
            pv = "{:.6f}".format(statistics.mean(par)) if par else "TIMEOUT"
            sv = "{:.6f}".format(statistics.mean(seq)) if seq else "TIMEOUT"
            print("{}\t{}\t{}\t{}\t{}\t{}".format(K, M, N, extra_args[-1], pv, sv), flush=True)

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--lab', required=True, choices=['openmp', 'opencl', 'mpi', 'cuda'])
    ap.add_argument('--dir', required=True)
    ap.add_argument('--binary', default='lab.elf')
    ap.add_argument('--runs', type=int, default=3)
    ap.add_argument('--timeout-par', type=int, default=120)
    ap.add_argument('--timeout-seq', type=int, default=60)
    
    ap.add_argument('-N', type=parse_range, default=[64, 128, 256])
    ap.add_argument('-k', type=parse_range, default=[64, 128, 256]) 
    ap.add_argument('-p', type=parse_range, default=[1, 2, 3, 4, 6])
    ap.add_argument('--type', nargs='+', default=['iso', 'sparse', 'noniso', 'reg'])
    ap.add_argument('--extra', nargs=argparse.REMAINDER, default=[])
    args = ap.parse_args()

    cwd = os.path.join(os.getcwd(), args.dir)
    if not os.path.isdir(cwd):
        print("Directory {} not found".format(args.dir), file=sys.stderr)
        sys.exit(1)

    try:
        build(args.dir, cwd)
        if args.lab == 'openmp':
            bench_openmp(args, args.dir)
        elif args.lab == 'opencl':
            bench_opencl(args, args.dir)
        elif args.lab == 'mpi':
            bench_mpi(args, args.dir)
        elif args.lab == 'cuda':
            bench_cuda(args, args.dir)
    finally:
        cleanup(args.dir, cwd)

if __name__ == '__main__':
    try:
        main()
    except KeyboardInterrupt:
        print("\n# Interrupted by user.", file=sys.stderr)
        sys.exit(130)