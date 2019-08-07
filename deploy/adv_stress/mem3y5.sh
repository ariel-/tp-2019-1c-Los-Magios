#!/bin/sh
IP2=IP_MEMORIA2
IP3=IP_MEMORIA3
IP4=IP_MEMORIA4
IPFS=IP_FS

echo PUERTO=8001 > memoria.conf
echo IP_FS=$IPFS >> memoria.conf
echo PUERTO_FS=5003 >> memoria.conf
echo IP_SEEDS=[$IP4,$IP2] >> memoria.conf
echo PUERTO_SEEDS=[8002,8001] >> memoria.conf
echo RETARDO_MEM=0 >> memoria.conf
echo RETARDO_FS=0 >> memoria.conf
echo TAM_MEM=256 >> memoria.conf
echo RETARDO_JOURNAL=70000 >> memoria.conf
echo RETARDO_GOSSIPING=30000 >> memoria.conf
echo MEMORY_NUMBER=3 >> memoria.conf

mkdir mem5
cd mem5
cp ../Memoria .

echo PUERTO=8002 > memoria.conf
echo IP_FS=$IPFS >> memoria.conf
echo PUERTO_FS=5003 >> memoria.conf
echo IP_SEEDS=[$IP2,$IP3] >> memoria.conf
echo PUERTO_SEEDS=[8001,8001] >> memoria.conf
echo RETARDO_MEM=0 >> memoria.conf
echo RETARDO_FS=0 >> memoria.conf
echo TAM_MEM=1024 >> memoria.conf
echo RETARDO_JOURNAL=50000 >> memoria.conf
echo RETARDO_GOSSIPING=30000 >> memoria.conf
echo MEMORY_NUMBER=5 >> memoria.conf

cd ..

tmux new-session -d './Memoria'
tmux split-window -p 66 '(cd mem5 && exec ./Memoria)'
tmux split-window -v 'htop'
tmux split-window -h
tmux attach-session -d
