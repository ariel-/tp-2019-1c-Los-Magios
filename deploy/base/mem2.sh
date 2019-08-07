#!/bin/sh
IPFS=IP_FS

echo PUERTO=8001 > memoria.conf
echo IP_FS=$IPFS >> memoria.conf
echo PUERTO_FS=5003 >> memoria.conf
echo IP_SEEDS=[] >> memoria.conf
echo PUERTO_SEEDS=[] >> memoria.conf
echo RETARDO_MEM=0 >> memoria.conf
echo RETARDO_FS=0 >> memoria.conf
echo TAM_MEM=1280 >> memoria.conf
echo RETARDO_JOURNAL=60000 >> memoria.conf
echo RETARDO_GOSSIPING=10000 >> memoria.conf
echo MEMORY_NUMBER=2 >> memoria.conf

tmux new-session -d './Memoria'
tmux split-window -v 'htop'
tmux split-window -h
tmux attach-session -d
