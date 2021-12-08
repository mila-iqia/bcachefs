#!/bin/sh
set -o errexit -o noclobber

interupt_message()
{
	ERR=$?
	echo
	echo "============================================"
	echo
	echo "The process was interrupted most likely because it hanged. To unmount the disk image, run the following commands:"
	echo "\\tpushd '${PWD}' && ./unmount.sh . && popd || popd"
	echo
	echo "============================================"
	echo
	exit ${ERR}
}

if [ -z "${UNMOUNT}" ]
then
	UNMOUNT=0
fi

trap interupt_message 1 2 3 6	# 1: SIGHUP
				# 2: SIGINT
				# 3: SIGQUIT
				# 6: SIGABRT

if [ ! -z "${RM_FAILED}" ]
then
	RM_FAILED=${RM_FAILED} singularity exec instance://bcachefs /bch/scripts/sif_rm_checksum_fail.sh && \
		singularity exec instance://bcachefs mv /bch/tmp/disk.img.md5sums.checksums /bch/tmp/disk.img.md5sums.checksums.failed
fi

for item in "$@"
do
	# singularity exec instance://bcachefs cp -aLu /bch/content/"${item}" /bch/mount/
	# Slower but copying one file at the time seams to reduce occurrence of fuse deadlocks
	singularity exec instance://bcachefs /bch/scripts/sif_cp.sh "${item}" || interupt_message
done
singularity exec instance://bcachefs /bch/scripts/sif_checksum.sh && [ ${UNMOUNT} -eq 1 ] && ./unmount.sh
_ERR=$?
if [ ! ${_ERR} -eq 0 ]
then
	echo
	echo "============================================"
	echo
	echo "Checksum verification failed. To delete corrupted files and retry the copy, run the following commands:"
	echo "\\tpushd '${PWD}' && UNMOUNT=${UNMOUNT} RM_FAILED=1 ./cp.sh . && popd || popd"
	echo
	echo "============================================"
	echo
fi
exit ${_ERR}
