#!/bin/bash
#./sub_jobs.sh bfs "basic partreduce partreducekvhint" "graph500" onenode "32M 64M 128M 256M 512M 1G 2G 4G 8G 16G 32G 64G" "65536 131072 262144 524288 1048576 2097152 4194304 8388608 16777216 33554432 67108864 134217728" 1 4006720

./sub_jobs.sh bfs "basic cpskvhint" "graph500s16" weekscale32M "32M 64M 128M 256M 512M 1G 2G" "33554432 67108864 134217728 268435456 536870912 1073741824 2147483648" 5 $1