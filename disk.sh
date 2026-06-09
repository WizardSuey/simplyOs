#!/bin/sh
# Этап 0 ФС: образ диска с MBR + раздел Linux ext2 (type 0x83).
set -e

DISK_IMG=disk.img
PART_IMG=.disk_part.img
DISK_MB=64
SRC=disk/root
BLOCK_SIZE=1024

if ! command -v sfdisk >/dev/null 2>&1; then
	echo "disk.sh: need sfdisk (util-linux)" >&2
	exit 1
fi

if ! command -v mkfs.ext2 >/dev/null 2>&1; then
	echo "disk.sh: need mkfs.ext2 (e2fsprogs)" >&2
	exit 1
fi

if ! command -v debugfs >/dev/null 2>&1; then
	echo "disk.sh: need debugfs (e2fsprogs)" >&2
	exit 1
fi

if [ ! -d "$SRC" ]; then
	echo "disk.sh: missing $SRC" >&2
	exit 1
fi

echo "Creating $DISK_IMG (${DISK_MB} MiB, MBR + ext2)..."

rm -f "$DISK_IMG" "$PART_IMG"
dd if=/dev/zero of="$DISK_IMG" bs=1M count="$DISK_MB" status=none

# Один primary-раздел Linux (0x83), выравнивание по умолчанию (обычно с сектора 2048).
printf '%s\n' ',,L,,' | sfdisk -q "$DISK_IMG"

PART_LINE=$(sfdisk -l "$DISK_IMG" -o START,END -n | awk '/^[[:space:]]+[0-9]+/ {print; exit}')
START=$(echo "$PART_LINE" | awk '{print $1}')
END=$(echo "$PART_LINE" | awk '{print $2}')
PART_SECTORS=$((END - START + 1))
PART_BYTES=$((PART_SECTORS * 512))
PART_MB=$(( (PART_BYTES + 1048575) / 1048576 ))

if [ -z "$START" ] || [ "$PART_SECTORS" -le 0 ]; then
	echo "disk.sh: failed to read partition layout" >&2
	exit 1
fi

echo "  partition 1: start LBA $START, size ${PART_SECTORS} sectors (~${PART_MB} MiB)"

dd if=/dev/zero of="$PART_IMG" bs=1M count="$PART_MB" status=none
mkfs.ext2 -b "$BLOCK_SIZE" -F -L MYOSROOT "$PART_IMG" 2>/dev/null

# Копируем файлы из disk/root/ на образ раздела (без mount/sudo).
debugfs_populate() {
	find "$SRC" -type d ! -path "$SRC" | sort | while read -r dir; do
		rel=${dir#$SRC}
		debugfs -w -R "mkdir ${rel}" "$PART_IMG" 2>/dev/null || true
	done

	find "$SRC" -type f | sort | while read -r file; do
		rel=${file#$SRC}
		debugfs -w -R "write ${file} ${rel}" "$PART_IMG" 2>/dev/null
	done
}

debugfs_populate

dd if="$PART_IMG" of="$DISK_IMG" bs=512 seek="$START" conv=notrunc status=none
rm -f "$PART_IMG"

# Проверка на хосте.
MAGIC=$(dd if="$DISK_IMG" bs=1 skip=$((START * 512 + 0x438)) count=2 status=none 2>/dev/null | od -An -tx1 | tr -d ' \n')
if [ "$MAGIC" != "53ef" ]; then
	echo "disk.sh: warning: ext2 magic at superblock+0x38 not 0xEF53 (got 0x$MAGIC)" >&2
fi

MBR_SIG=$(dd if="$DISK_IMG" bs=1 skip=510 count=2 status=none | od -An -tx1 | tr -d ' \n')
if [ "$MBR_SIG" != "55aa" ]; then
	echo "disk.sh: warning: MBR signature not 0x55AA (got 0x$MBR_SIG)" >&2
fi

echo "Done: $DISK_IMG"
echo "  MBR signature: 0x${MBR_SIG:-??}"
echo "  ext2 label: MYOSROOT, block size: ${BLOCK_SIZE}"
echo "  test files: /hello.txt, /readme.txt, /docs/note.txt"
echo "Host check: debugfs -R 'ls -l /' $DISK_IMG  # after extracting part, or use loop mount"
