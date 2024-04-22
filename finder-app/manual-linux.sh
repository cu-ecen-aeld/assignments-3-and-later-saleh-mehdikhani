#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo.

set -e
set -u

OUTDIR=/tmp/aeld
KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v5.1.10
BUSYBOX_VERSION=1_33_1
FINDER_APP_DIR=$(realpath $(dirname $0))
ARCH=arm64
CROSS_COMPILE=aarch64-none-linux-gnu-

if [ $# -lt 1 ]
then
	echo "Using default directory ${OUTDIR} for output"
else
	OUTDIR=$1
	echo "Using passed directory ${OUTDIR} for output"
fi

mkdir -p ${OUTDIR}

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/linux-stable" ]; then
    #Clone only if the repository does not exist.
	echo "CLONING GIT LINUX STABLE VERSION ${KERNEL_VERSION} IN ${OUTDIR}"
	git clone ${KERNEL_REPO} --depth 1 --single-branch --branch ${KERNEL_VERSION}
fi
if [ ! -e ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ]; then
    cd linux-stable
    echo "Checking out version ${KERNEL_VERSION}"
    git checkout ${KERNEL_VERSION}

    # TODO: Add your kernel build steps here
    # Deep clean
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} mrproper
    # Create .config file for virt arm QEMU
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} defconfig
    # Build vmlinux (kernel image)
    make -j4 ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} all
    # Build kernel modules
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} modules
    # Build the device tree
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} dtbs
fi

echo "Adding the Image in outdir"
cp ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ${OUTDIR}

echo "Creating the staging directory for the root filesystem"
cd "$OUTDIR"
if [ -d "${OUTDIR}/rootfs" ]
then
	echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    sudo rm  -rf ${OUTDIR}/rootfs
fi

# TODO: Create necessary base directories
mkdir ${OUTDIR}/rootfs && cd ${OUTDIR}/rootfs
mkdir -p bin dev etc home lib lib64 proc sbin sys tmp usr var
mkdir -p usr/bin usr/lib usr/sbin
mkdir -p var/log

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/busybox" ]
then
git clone git://busybox.net/busybox.git
    cd busybox
    git checkout ${BUSYBOX_VERSION}
    # TODO:  Configure busybox
    make distclean
    make defconfig
else
    cd busybox
fi

# TODO: Make and install busybox
make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE}
make CONFIG_PREFIX="${OUTDIR}/rootfs" ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} install
cd "${OUTDIR}/rootfs"

echo "Library dependencies"
${CROSS_COMPILE}readelf -a bin/busybox | grep "program interpreter"
${CROSS_COMPILE}readelf -a bin/busybox | grep "Shared library"

# TODO: Add library dependencies to rootfs
# The root directory of the gcc toolchain
TOOLCHAIN_DIR=$(dirname $(dirname $(which ${CROSS_COMPILE}readelf)))
# Find and copy interpreter files
grep_program_interpreter=$(${CROSS_COMPILE}readelf -a bin/busybox | grep "program interpreter")
Prog_Interpreter=$(echo "$grep_program_interpreter" | sed -n 's/.*: \(.*\)]/\1/p')
IFS=$'\n'
for path in ${Prog_Interpreter}; do
    find ${TOOLCHAIN_DIR} -name "$(basename ${path})" | xargs -I {} cp {} "${OUTDIR}/rootfs/lib"
done
# Find and copy shared library files
grep_shared_library=$(${CROSS_COMPILE}readelf -a bin/busybox | grep "Shared library")
Shared_Libs=$(echo "$grep_shared_library" | sed -n 's/.*\[\(.*\)\]/\1/p')
IFS=$'\n'
for path in ${Shared_Libs}; do
    find ${TOOLCHAIN_DIR} -name "$(basename ${path})" | xargs -I {} cp {} "${OUTDIR}/rootfs/lib64"
done

# TODO: Make device nodes
cd "${OUTDIR}/rootfs"
sudo mknod -m 666 dev/null c 1 3
sudo mknod -m 666 dev/console c 5 1

# TODO: Clean and build the writer utility
cd ${FINDER_APP_DIR}
make clean
make CROSS_COMPILE=${CROSS_COMPILE}

# TODO: Copy the finder related scripts and executables to the /home directory
# on the target rootfs
cp "${FINDER_APP_DIR}"/* "${OUTDIR}/rootfs/home"
mkdir "${OUTDIR}"/rootfs/home/conf
cp "${FINDER_APP_DIR}"/conf/*.txt "${OUTDIR}/rootfs/home/conf"

# TODO: Chown the root directory
cd "${OUTDIR}/rootfs/"
sudo chown -R root:root *

# TODO: Create initramfs.cpio.gz
find . | cpio -H newc -ov --owner root:root > "${OUTDIR}"/initramfs.cpio
cd "${OUTDIR}"
gzip -f initramfs.cpio
