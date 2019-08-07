#!/bin/sh
IP1=IP_MEMORIA1
IPFS=IP_FS

echo PUERTO=8001 > memoria.conf
echo IP_FS=$IPFS >> memoria.conf
echo PUERTO_FS=5003 >> memoria.conf
echo IP_SEEDS=[$IP1] >> memoria.conf
echo PUERTO_SEEDS=[8001] >> memoria.conf
echo RETARDO_MEM=0 >> memoria.conf
echo RETARDO_FS=0 >> memoria.conf
echo TAM_MEM=2048 >> memoria.conf
echo RETARDO_JOURNAL=30000 >> memoria.conf
echo RETARDO_GOSSIPING=30000 >> memoria.conf
echo MEMORY_NUMBER=2 >> memoria.conf

mkdir mem4
cd mem4
cp ../Memoria .

echo PUERTO=8002 > memoria.conf
echo IP_FS=$IPFS >> memoria.conf
echo PUERTO_FS=5003 >> memoria.conf
echo IP_SEEDS=[$IP1] >> memoria.conf
echo PUERTO_SEEDS=[8001] >> memoria.conf
echo RETARDO_MEM=0 >> memoria.conf
echo RETARDO_FS=0 >> memoria.conf
echo TAM_MEM=4096 >> memoria.conf
echo RETARDO_JOURNAL=30000 >> memoria.conf
echo RETARDO_GOSSIPING=30000 >> memoria.conf
echo MEMORY_NUMBER=4 >> memoria.conf

cd ..

tmux new-session -d './Memoria'
tmux split-window -p 66 '(cd mem4 && exec ./Memoria)'
tmux split-window -v 'htop'
tmux split-window -h
tmux attach-session -d
