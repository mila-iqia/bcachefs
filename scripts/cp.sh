#!/bin/sh
set -o errexit -o noclobber

if [ -z "${UNMOUNT}" ]
then
	UNMOUNT=0
fi

if [ ! -z "${RM_FAILED}" ]
then
	RM_FAILED=${RM_FAILED} singularity exec instance://bcachefs /bch/scripts/sif_rm_checksum_fail.sh && \
		singularity exec instance://bcachefs mv /bch/tmp/disk.img.md5sums.checksums /bch/tmp/disk.img.md5sums.checksums.failed
fi

for item in "$@"
do
	# singularity exec instance://bcachefs cp -aLu /bch/content/"${item}" /bch/mount/
	# Slower but copying one file at the time seams to reduce occurrence of fuse deadlocks
	singularity exec instance://bcachefs /bch/scripts/sif_cp.sh "${item}"
done
singularity exec instance://bcachefs /bch/scripts/sif_checksum.sh && [ ${UNMOUNT} -eq 1 ] && ./unmount.sh
_ERR=$?
if [ ! ${_ERR} -eq 0 ]
then
	echo
	echo "Checksum verification failed. To delete corrupted files and retry the copy, run the following commands:"
	echo -e "\\tpushd '${PWD}' && UNMOUNT=${UNMOUNT} RM_FAILED=1 ./cp.sh . ; popd"
	echo
fi
exit ${_ERR}
