#!/bin/sh
set -o errexit -o noclobber

echo
echo "============================================"
echo
echo "Unmounting ..."
echo
echo "============================================"
echo
singularity exec instance://bcachefs fusermount3 -u /bch/mount/ && sleep 10
singularity instance stop bcachefs
