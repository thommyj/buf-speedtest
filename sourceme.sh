#!/bin/sh

export KERNEL_SRC=`pwd`/../linux/
export CCPREFIX=arm-linux-gnueabihf-
export ARCH=arm 
export CROSS_COMPILE=${CCPREFIX}

