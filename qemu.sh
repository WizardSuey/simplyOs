#!/bin/sh
set -e
. ./iso.sh

if [ ! -f disk.img ]; then
	./disk.sh
fi

qemu-system-$(./target-triplet-to-arch.sh $HOST) -serial file:serial.log -cdrom myos.iso -hda disk.img
