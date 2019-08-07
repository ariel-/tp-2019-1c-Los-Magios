#!/bin/sh
IP3=IP_MEMORIA3

echo IP_MEMORIA=$IP3 > kernel.conf
echo PUERTO_MEMORIA=8001 >> kernel.conf
echo QUANTUM=1 >> kernel.conf
echo MULTIPROCESAMIENTO=1 >> kernel.conf
echo METADATA_REFRESH=15000 >> kernel.conf
echo SLEEP_EJECUCION=1000 >> kernel.conf

tmux new-session -d './Kernel'
tmux split-window -v 'htop'
tmux split-window -h
tmux attach-session -d
