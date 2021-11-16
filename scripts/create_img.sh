#!/bin/sh
set -o errexit -o noclobber

if [ -z "${NAME}" ]
then
	NAME=bcachefs.img
fi

if [ -z "${SIZE}" ]
then
	SIZE=10MiB
fi

if [ -e "${NAME}" ]
then
	exit 1
fi

truncate -s ${SIZE} "${NAME}"
singularity run -B "${NAME}":/bch/disk.img:rw \
	bcachefs-tools.sif format --block_size=4k --metadata_checksum=none --data_checksum=none --compression=none --str_hash=siphash --label=LabelDEADBEEF /bch/disk.img
