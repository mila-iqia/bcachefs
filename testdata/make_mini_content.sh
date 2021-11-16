#!/bin/bash

_CWD=${PWD}

pushd `dirname "${BASH_SOURCE[0]}"` >/dev/null

mkdir -p .tmp/

PREFIX=mini
NAME=${PREFIX}_bcachefs.img
SIZE=

# Create data
rm -rf .tmp/*
unzip ${PREFIX}_content.zip -d .tmp/
cp -a .tmp/${PREFIX}_content.md5sums "${_CWD}/${NAME}".md5sums

# Compute disk image size
SIZE=$(du -s --block-size=1 .tmp/${PREFIX}_content/ | tail -n 1 | cut -f 1)
if [[ ${SIZE} -lt 10485760 ]] # 10 MiB
then
	SIZE=10485760
fi

# Create disk image and copy content
NAME="${_CWD}/${NAME}" \
	CONTENT_SRC=.tmp/${PREFIX}_content/ \
	SIZE=${SIZE} \
	TMP_DIR=.tmp/ \
	scripts/make_disk_image.sh

popd >/dev/null
