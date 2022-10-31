#!/bin/sh
set -o errexit -o noclobber

cd /bch/mount/
grep ": FAILED" /bch/tmp/disk.img.md5sums.checksums | sed "s|: FAILED||" | xargs rm || [ -z "$(grep ": FAILED" /bch/tmp/disk.img.md5sums.checksums)" ]
