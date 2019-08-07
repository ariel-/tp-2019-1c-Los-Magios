#!/bin/sh
IPFS=IP_FS

MOUNTPOINT="/home/utnso/lfs-stress/"

echo PUERTO_ESCUCHA=5003 > lissandra.conf
echo PUNTO_MONTAJE=$MOUNTPOINT >> lissandra.conf
echo RETARDO=0 >> lissandra.conf
echo TAMANIO_VALUE=60 >> lissandra.conf
echo TIEMPO_DUMP=20000 >> lissandra.conf
echo BLOCK_SIZE=128 >> lissandra.conf
echo BLOCKS=8192 >> lissandra.conf

echo PUERTO=8001 > memoria.conf
echo IP_FS=$IPFS >> memoria.conf
echo PUERTO_FS=5003 >> memoria.conf
echo IP_SEEDS=[] >> memoria.conf
echo PUERTO_SEEDS=[] >> memoria.conf
echo RETARDO_MEM=0 >> memoria.conf
echo RETARDO_FS=0 >> memoria.conf
echo TAM_MEM=4096 >> memoria.conf
echo RETARDO_JOURNAL=70000 >> memoria.conf
echo RETARDO_GOSSIPING=30000 >> memoria.conf
echo MEMORY_NUMBER=1 >> memoria.conf

tmux new-session -d './LFS'
sleep 1
tmux split-window -p 66 './Memoria'
tmux split-window -v 'htop'
tmux split-window -h
tmux attach-session -d
