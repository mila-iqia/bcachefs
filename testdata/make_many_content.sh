#!/bin/bash

_CWD=${PWD}

if [ -z "${CLEANUP}" ]
then
	CLEANUP=0
fi

pushd `dirname "${BASH_SOURCE[0]}"` >/dev/null
_SCRIPT_DIR=`pwd -P`

mkdir -p .tmp/

PREFIX=many
NAME=${PREFIX}_bcachefs.img
SIZE=

# Clean-up traces of previous data
[[ ${CLEANUP} -eq 1 ]] && rm -rf .tmp/*

# Create data
mkdir -p .tmp/${PREFIX}_content/
[[ -e ".tmp/${PREFIX}_content/0" ]] || echo "test content" > .tmp/${PREFIX}_content/0

for i in {1..25}
do
	mkdir -p .tmp/${PREFIX}_content/$i/
	for j in {1..1500}
	do
		[[ -e ".tmp/${PREFIX}_content/$i/$(( ($i - 1) * 1500 + $j))" ]] || \
			ln .tmp/${PREFIX}_content/0 .tmp/${PREFIX}_content/$i/$(( ($i - 1) * 1500 + $j))
	done
done

# Compute disk image size
SIZE=$(du -s --block-size=1 .tmp/${PREFIX}_content/ | tail -n 1 | cut -f 1)
if [[ ${SIZE} -lt 31457280 ]] # 30 MiB: a minimum space is required to store the dirents
then
	SIZE=31457280
fi
SIZE=$(numfmt --to=iec-i --suffix=B ${SIZE})

# Create disk image and copy content
NAME="${_CWD}/${NAME}" \
	CONTENT_SRC=.tmp/${PREFIX}_content/ \
	SIZE=${SIZE} \
	TMP_DIR=.tmp/ \
	scripts/make_disk_image.sh
