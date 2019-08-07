#!/bin/sh
IPFS=IP_FS
IP2=IP_MEMORIA2

echo PUERTO=8001 > memoria.conf
echo IP_FS=$IPFS >> memoria.conf
echo PUERTO_FS=5003 >> memoria.conf
echo IP_SEEDS=[$IP2] >> memoria.conf
echo PUERTO_SEEDS=[8001] >> memoria.conf
echo RETARDO_MEM=0 >> memoria.conf
echo RETARDO_FS=0 >> memoria.conf
echo TAM_MEM=320 >> memoria.conf
echo RETARDO_JOURNAL=60000 >> memoria.conf
echo RETARDO_GOSSIPING=10000 >> memoria.conf
echo MEMORY_NUMBER=1 >> memoria.conf

tmux new-session -d './Memoria'
tmux split-window -v 'htop'
tmux split-window -h
tmux attach-session -d
