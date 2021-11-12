#!/bin/sh
set -o errexit -o noclobber

cd /bch/mount/
md5sum -c /bch/disk.img.md5sums > /bch/tmp/disk.img.md5sums.checksums
