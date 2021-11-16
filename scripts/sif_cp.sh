#!/bin/sh
set -o errexit -o noclobber

cd /bch/content/
for item in "$@"
do
	find ${item} -type d -exec mkdir -p /bch/mount/"{}" \; \
		-o -type f -exec cp -aLu "{}" /bch/mount/"{}" \;
done
