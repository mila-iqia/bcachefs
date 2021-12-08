#!/bin/sh
set -o errexit -o noclobber

if [ -z "${RM_FAILED}" ]
then
	RM_FAILED=0
fi

cd /bch/mount/
[ ${RM_FAILED} -eq 1 ] && [ -z "`grep ": FAILED" /bch/tmp/disk.img.md5sums.checksums`" ] || \
	grep ": FAILED" /bch/tmp/disk.img.md5sums.checksums | sed "s|: FAILED||" | xargs rm
