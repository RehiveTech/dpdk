#! /bin/sh

uevent_create()
{
	echo "OF_FULLNAME=/rootbus/testdevice"
	echo "OF_COMPATIBLE_N=1"
	echo "OF_COMPATIBLE_0=dpdk,test-device"
}

setup_sys()
{
	cd ${1}

	mkdir -p bus/platform/devices
	cd bus/platform/devices

	mkdir dpdk@testdevice
	uevent_create > dpdk@testdevice/uevent

	cd ${2}
}

setup_fdt()
{
	cd ${1}

	mkdir rootbus
	mkdir rootbus/testdevice
	echo -e 'dpdk,test-device\0' > rootbus/testdevice/compatible
	echo -e 'test-device\0' > rootbus/testdevice/name

	cd ${2}
}

setup()
{
	mkdir sys
	mkdir device-tree

	cur=`pwd`
	sys=$cur/sys
	fdt=$cur/device-tree

	setup_sys "$sys" "$cur"
	setup_fdt "$fdt" "$cur"
}

clean()
{
	rm -rf device-tree
	rm -rf sys
}

if [ "${1}" = "setup" ]; then
	setup
elif [ "${1}" = "clean" ]; then
	clean
else
	echo "Specify either 'setup' or 'clean' command" >&2
	exit -1
fi
