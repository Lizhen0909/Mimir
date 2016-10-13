#! /bin/zsh

BDIR="/projects/SSSPPg/yguo/mt-mrmpi/data"

function create_data {
    N_LIGAND=$(echo "2^$1" | bc)
    node=$2
    qsub -A MPICH_MCS -t 15 -n $node --mode script \
        ./octreegenerator.bgq.job octree \
        "$BDIR/octree/weakscale$1/$node" \
        0 0 0.5 "$N_LIGAND" normal $node
}

SCALE=$1
n_start=$2
n_end=$3

for ((n=n_start; n<=n_end; n=n*2)); do
    create_data $SCALE $n
    while [ $? -ne 0 ]; do
        echo "wait for 5m"
        sleep 5m
        create_data $SCALE $n
    done
done