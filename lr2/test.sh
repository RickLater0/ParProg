#!/bin/bash
set -u

N=${1:-50}
TYPE=${2:-iso}
LAB=${3:-./lab.elf}
M1=m1.mtrx
M2=m2.mtrx

if ! [ -x "$LAB" ]; then
    echo "error: $LAB not found" >&2
    exit 1
fi

case "$TYPE" in
    iso|sparse|noniso|reg) ;;
    *)
        echo "usage: $0 <n> <iso|sparse|noniso|reg> [lab]" >&2
        echo "  iso    - permuted dense random" >&2
        echo "  sparse - permuted sparse random (parallel wins here)" >&2
        echo "  noniso - two independent dense random" >&2
        echo "  reg    - permuted 3-regular" >&2
        exit 1
        ;;
esac

gen_dense() {
    awk -v n="$N" -v seed="$1" 'BEGIN{
        srand(seed);
        for(i=0;i<n;i++) for(j=0;j<n;j++) m[i,j]=0;
        for(i=0;i<n;i++) for(j=i+1;j<n;j++){
            if(rand()<0.5){ m[i,j]=1; m[j,i]=1; }
        }
        for(i=0;i<n;i++){ for(j=0;j<n;j++) printf "%d ", m[i,j]; printf "\n"; }
    }'
}

gen_sparse() {
    awk -v n="$N" -v seed="$1" 'BEGIN{
        srand(seed);
        p = 2.0 / n;
        for(i=0;i<n;i++) for(j=0;j<n;j++) m[i,j]=0;
        for(i=0;i<n;i++) for(j=i+1;j<n;j++){
            if(rand()<p){ m[i,j]=1; m[j,i]=1; }
        }
        for(i=0;i<n;i++){ for(j=0;j<n;j++) printf "%d ", m[i,j]; printf "\n"; }
    }'
}

gen_regular() {
    awk -v n="$N" -v seed="$1" 'BEGIN{
        if(n%2){ print "reg: n must be even" > "/dev/stderr"; exit 1; }
        srand(seed);
        for(i=0;i<n;i++) for(j=0;j<n;j++) m[i,j]=0;
        for(i=0;i<n;i++){ m[i,(i+1)%n]=1; m[(i+1)%n,i]=1; }
        h=n/2;
        for(i=0;i<h;i++){ m[i,i+h]=1; m[i+h,i]=1; }
        for(i=0;i<n;i++){ for(j=0;j<n;j++) printf "%d ", m[i,j]; printf "\n"; }
    }'
}

gen_permute() {
    awk -v n="$N" -v seed="$1" '
    BEGIN{
        srand(seed);
        for(i=0;i<n;i++) p[i]=i;
        for(i=n-1;i>0;i--){ k=int(rand()*(i+1)); t=p[i]; p[i]=p[k]; p[k]=t; }
        idx=0;
    }
    { for(i=1;i<=NF;i++) a[idx++]=$i; }
    END{
        if(idx != n*n){ print "permute: bad size" > "/dev/stderr"; exit 1; }
        for(i=0;i<n;i++){ for(j=0;j<n;j++) printf "%d ", a[p[i]*n+p[j]]; printf "\n"; }
    }'
}

case "$TYPE" in
    iso)
        gen_dense "$N" 100 > "$M1"
        gen_permute "$N" 200 < "$M1" > "$M2" || exit 1
        ;;
    sparse)
        gen_sparse "$N" 100 > "$M1"
        gen_permute "$N" 200 < "$M1" > "$M2" || exit 1
        ;;
    noniso)
        gen_dense "$N" 100 > "$M1"
        gen_dense "$N" 200 > "$M2"
        ;;
    reg)
        gen_regular "$N" 100 > "$M1"
        gen_permute "$N" 200 < "$M1" > "$M2" || exit 1
        ;;
esac

echo "[test] N=$N TYPE=$TYPE"
echo "[test] m1: $M1 ($(wc -c < "$M1") bytes)"
echo "[test] m2: $M2 ($(wc -c < "$M2") bytes)"
echo

"$LAB" "$N" "$M1" "$M2"