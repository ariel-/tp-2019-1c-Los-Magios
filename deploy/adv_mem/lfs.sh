#!/bin/sh
MOUNTPOINT="/home/utnso/prueba-memoria/"

echo PUERTO_ESCUCHA=5003 > lissandra.conf
echo PUNTO_MONTAJE=$MOUNTPOINT >> lissandra.conf
echo RETARDO=0 >> lissandra.conf
echo TAMANIO_VALUE=54 >> lissandra.conf
echo TIEMPO_DUMP=3000 >> lissandra.conf
echo BLOCK_SIZE=64 >> lissandra.conf
echo BLOCKS=4096 >> lissandra.conf

tmux new-session -d './LFS'
tmux split-window -v 'htop'
tmux split-window -h
tmux attach-session -d
