#! /bin/zsh

# ./sub_jobs.sh wordcount "basic partreduce partreducekvhint" \
#   "uniform" "weakscale_small" \
#   "32M" \
#   "" 1 none

#./sub_jobs.sh basic wordcount uniform/weakscale_small/32M 2 1 4 "no param"
#./sub_jobs.sh basic wordcount uniform/weakscale_small/32M 5 1 1 512M
# ./sub_jobs.sh basic wordcount uniform/weakscale_small/64M 1 1 1
# ./sub_jobs.sh basic wordcount uniform/weakscale_small/128M 1 1 1
# ./sub_jobs.sh basic wordcount uniform/weakscale_small/256M 1 1 1
# ./sub_jobs.sh basic wordcount uniform/weakscale_small/512M 1 1 1
# ./sub_jobs.sh basic wordcount uniform/weakscale_small/1024M 1 1 1
#./sub_jobs.sh basic wordcount uniform/weakscale_small/4G 1 1 1 256M
#./sub_jobs.sh cps wordcount uniform/weakscale_small/4G 1 1 1 256M
#./sub_jobs.sh cps wordcount uniform/weakscale_small/8G 1 1 1 128M
#./sub_jobs.sh basic wordcount uniform/weakscale_small/4G 1 1 1 64M
#./sub_jobs.sh basic wordcount uniform/weakscale_small/4G 1 1 1 64M
#./sub_jobs.sh basic wordcount uniform/weakscale_small/4G 1 1 1 64M
#./sub_jobs.sh basic wordcount uniform/weakscale_small/4G 1 1 1 64M
# ./sub_jobs.sh basic wordcount uniform/weakscale_small/4G 1 1 1
# ./sub_jobs.sh basic wordcount uniform/weakscale_small/8G 1 1 1

#./sub_jobs.sh pr wordcount uniform/weakscale_small/2G 1 1 1 512M
./sub_jobs.sh pr wordcount uniform/weakscale_small/2G 1 1 1 256M
./sub_jobs.sh pr wordcount uniform/weakscale_small/2G 1 1 1 128M
#./sub_jobs.sh pr wordcount uniform/weakscale_small/4G 1 1 1 512M
#./sub_jobs.sh pr wordcount uniform/weakscale_small/4G 1 1 1 256M
#./sub_jobs.sh pr wordcount uniform/weakscale_small/8G 1 1 1 256M
#./sub_jobs.sh pr wordcount uniform/weakscale_small/8G 1 1 1 128M
#
# ./sub_jobs.sh partreduce wordcount uniform/weakscale_small/32M 1 1 1
# ./sub_jobs.sh partreduce wordcount uniform/weakscale_small/64M 1 1 1
# ./sub_jobs.sh partreduce wordcount uniform/weakscale_small/128M 1 1 1
# ./sub_jobs.sh partreduce wordcount uniform/weakscale_small/256M 1 1 1
# ./sub_jobs.sh partreduce wordcount uniform/weakscale_small/512M 1 1 1
# ./sub_jobs.sh partreduce wordcount uniform/weakscale_small/1024M 1 1 1
# ./sub_jobs.sh partreduce wordcount uniform/weakscale_small/2G 1 1 1
# ./sub_jobs.sh partreduce wordcount uniform/weakscale_small/4G 1 1 1
# ./sub_jobs.sh partreduce wordcount uniform/weakscale_small/8G 1 1 1
# 
# ./sub_jobs.sh partreducekvhint wordcount uniform/weakscale_small/32M 1 1 1
# ./sub_jobs.sh partreducekvhint wordcount uniform/weakscale_small/64M 1 1 1
# ./sub_jobs.sh partreducekvhint wordcount uniform/weakscale_small/128M 1 1 1
# ./sub_jobs.sh partreducekvhint wordcount uniform/weakscale_small/256M 1 1 1
# ./sub_jobs.sh partreducekvhint wordcount uniform/weakscale_small/512M 1 1 1
# ./sub_jobs.sh partreducekvhint wordcount uniform/weakscale_small/1024M 1 1 1
# ./sub_jobs.sh partreducekvhint wordcount uniform/weakscale_small/2G 1 1 1
# ./sub_jobs.sh partreducekvhint wordcount uniform/weakscale_small/4G 1 1 1
# ./sub_jobs.sh partreducekvhint wordcount uniform/weakscale_small/8G 1 1 1
