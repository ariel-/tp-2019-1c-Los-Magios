#!/bin/sh
IPFS=IP_FS
IP3=IP_MEMORIA3

MOUNTPOINT="/home/utnso/lfs-compactacion/"

echo PUERTO_ESCUCHA=5003 > lissandra.conf
echo PUNTO_MONTAJE=$MOUNTPOINT >> lissandra.conf
echo RETARDO=0 >> lissandra.conf
echo TAMANIO_VALUE=54 >> lissandra.conf
echo TIEMPO_DUMP=5000 >> lissandra.conf
echo BLOCK_SIZE=64 >> lissandra.conf
echo BLOCKS=4096 >> lissandra.conf

echo PUERTO=8001 > memoria.conf
echo IP_FS=$IPFS >> memoria.conf
echo PUERTO_FS=5003 >> memoria.conf
echo IP_SEEDS=[$IP3] >> memoria.conf
echo PUERTO_SEEDS=[8001] >> memoria.conf
echo RETARDO_MEM=0 >> memoria.conf
echo RETARDO_FS=0 >> memoria.conf
echo TAM_MEM=320 >> memoria.conf
echo RETARDO_JOURNAL=60000 >> memoria.conf
echo RETARDO_GOSSIPING=10000 >> memoria.conf
echo MEMORY_NUMBER=2 >> memoria.conf

tmux new-session -d './LFS'
sleep 1
tmux split-window -p 66 './Memoria'
tmux split-window -v 'htop'
tmux split-window -h
tmux attach-session -d
