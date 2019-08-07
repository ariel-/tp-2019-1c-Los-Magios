#!/bin/sh
IP5=IP_MEMORIA5

echo IP_MEMORIA=$IP5 > kernel.conf
echo PUERTO_MEMORIA=8002 >> kernel.conf
echo QUANTUM=1 >> kernel.conf
echo MULTIPROCESAMIENTO=3 >> kernel.conf
echo METADATA_REFRESH=15000 >> kernel.conf
echo SLEEP_EJECUCION=10 >> kernel.conf

tmux new-session -d './Kernel'
tmux split-window -v 'htop'
tmux split-window -h
tmux attach-session -d
