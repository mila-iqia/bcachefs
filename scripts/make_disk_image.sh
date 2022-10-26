#!/bin/bash
set -m

if [ -z "${NAME}" ]
then
	NAME=${PWD}/bcachefs.img
fi

if [ -z "${CONTENT_SRC}" ]
then
	CONTENT_SRC=${PWD}
fi

if [ -z "${SIZE}" ]
then
	SIZE=
fi

if [ -z "${TMP_DIR}" ]
then
	TMP_DIR=${PWD}/tmp/
fi

NAME=`realpath "${NAME}"`
CONTENT_SRC=`realpath "${CONTENT_SRC}"`
TMP_DIR=`realpath "${TMP_DIR}"`

mkdir -p "${TMP_DIR}"/

# Compute disk image size
if [ -z "${SIZE}" ]
then
	SIZE=$(du -s --block-size=1 "${CONTENT_SRC}"/ | tail -n 1 | cut -f 1)
	SIZE=$(( ${SIZE} + ${SIZE} / 5 )) # +20% to avoid no more space left
	if [[ ${SIZE} -lt 10485760 ]] # 10 MiB
	then
		SIZE=10485760
	fi
	SIZE=$(numfmt --to=iec-i --suffix=B ${SIZE})
fi

if [[ ! -f "${NAME}".md5sums ]]
then
	pushd "${CONTENT_SRC}" >/dev/null
	find -type f -exec md5sum {} + > "${NAME}".md5sums
	sed -i "s|  \./|  |" "${NAME}".md5sums
	popd >/dev/null
fi

pushd `dirname "${BASH_SOURCE[0]}"` >/dev/null

chmod +x bcachefs-tools.sif
if [[ ${SIZE} != -1 ]]
then
	# Create disk image
	NAME="${NAME}" SIZE=${SIZE} ./create_img.sh
fi
# Mount disk image
NAME="${NAME}" \
	CONTENT_SRC=${CONTENT_SRC}/ \
	SCRIPTS_DIR=./ \
	TMP_DIR=${TMP_DIR}/ \
	./mount.sh &

sleep 2

echo
echo "============================================"
echo
echo "Run the following commands in another shell:"
echo -e "\\tpushd '${PWD}' && UNMOUNT=1 $([ ${RM_FAILED} == 1 ] && echo "RM_FAILED=1 ")./cp.sh . ; popd"
echo
echo "============================================"
echo

popd >/dev/null

fg %1 >/dev/null
