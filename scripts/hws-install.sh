#!/bin/sh

LOGFILE=hws_install.log

if [ -h $0 ]; then
    SCRIPT_PATH=`readlink $0 | xargs dirname`
else
    SCRIPT_PATH="$(cd "$(dirname "$0")" && pwd)"
fi
HWS_TOP_DIR=$SCRIPT_PATH/..
SRC_DIR=$HWS_TOP_DIR/src
MODULE_NAME=HwsUHDX1Capture.ko
MODUEL_INSTALL_DIR=/usr/local/share/HWS

	echo $SRC_DIR
	echo $HWS_TOP_DIR

MODULE_BUILD_DIR="`pwd`/hws_build"

ARCH=`uname -m | sed -e 's/i.86/i386/'`
case $ARCH in
	i386) ARCH_BITS=32 ;;
	arm*) ARCH_BITS=arm ;;
	aarch64) ARCH_BITS=aarch64 ;;
	*) ARCH_BITS=64 ;;
esac

echo_string ()
{
    echo "$1" | tee -a $LOGFILE
}

echo_string_nonewline ()
{
    echo -n "$1" | tee -a $LOGFILE
}

error_exit ()
{
    echo ""
    echo "Please check $LOGFILE for more details."
    echo "If you are experiencing difficulty with this installation"
    echo "please contact  sales@avmatrix.com"
    exit 1
}

build_prepare ()
{
    if [ -d $MODULE_BUILD_DIR ]; then
        echo_string "Build directory: $MODULE_BUILD_DIR already exists."
        echo_string "Do you wish to remove $MODULE_BUILD_DIR (Y/N) [N]: "
        read cont

        if [ "$cont" != "YES" -a "$cont" != "yes" -a \
            "$cont" != "Y" -a "$cont" != "y" ] ; then
            echo
            echo "Cancelling installation, $MODULE_BUILD_DIR unmodified."
            exit
        fi

        echo_string_nonewline "Removing directory $MODULE_BUILD_DIR ... "
        rm -rvf $MODULE_BUILD_DIR >> $LOGFILE 2>&1
        RET=$?
        if [ $RET -ne 0 ]; then
            echo_string "ERROR: Failed to remove directory:"
            echo_string "   $MODULE_BUILD_DIR"
            echo_string "You should remove it manually."
            error_exit
        fi
        echo_string "Done."
    fi

    echo_string_nonewline "Creating build directory $MODULE_BUILD_DIR ... "
    mkdir -p $MODULE_BUILD_DIR >> $LOGFILE 2>&1
    RET=$?
    if [ $RET -ne 0 ]; then
        echo_string ""
        echo_string "ERROR: Failed to create build directory $MODULE_BUILD_DIR"
        error_exit
    fi
    echo_string "Done."

    echo_string_nonewline "Copying driver source into $MODULE_BUILD_DIR ... "
    cp -avf $SRC_DIR/. $MODULE_BUILD_DIR >> $LOGFILE 2>&1
    RET=$?
    if [ $RET -ne 0 ]; then
        echo_string ""
        echo_string "ERROR: Failed to copy driver source into $MODULE_BUILD_DIR"
        error_exit
    fi
    echo_string "Done."
}

build_clean ()
{
    if [ -d $MODULE_BUILD_DIR ]; then
        echo_string_nonewline "Removing build directory $MODULE_BUILD_DIR ... "
        rm -rvf $MODULE_BUILD_DIR >> $LOGFILE 2>&1
        RET=$?
        if [ $RET -ne 0 ]; then
            echo_string "Warning: Failed to remove build directory:"
            echo_string "   $MODULE_BUILD_DIR"
            echo_string "You should remove it manually."
        fi
        echo_string "Done."
    fi
}

clean_module ()
{
    echo_string_nonewline "Cleaning build ... "
    if [ -d $MODULE_BUILD_DIR ]; then
        make -C $MODULE_BUILD_DIR clean >> $LOGFILE 2>&1
    fi
    echo_string "Done."
}

build_module ()
{
    echo_string_nonewline "Building module for kernel `uname -r` ... "
    make -C $MODULE_BUILD_DIR -j4 >> $LOGFILE 2>&1
    RET=$?
    if [ $RET -ne 0 ] ; then
        echo_string ""
        echo_string "ERROR: Failed to build module!"
        error_exit
    fi
    echo_string "Done."
}

install_module ()
{
    echo_string_nonewline "Installing module ... "
    make -C $MODULE_BUILD_DIR install -j4 >> $LOGFILE 2>&1
    RET=$?
    if [ $RET -ne 0 ] ; then
        echo_string ""
        echo_string "ERROR: Failed to install module!"
        error_exit
    fi
    if [ ! -d $MODUEL_INSTALL_DIR ]; then
        mkdir -p $MODUEL_INSTALL_DIR >> $LOGFILE 2>&1
    fi
    RET=$?
    if [ $RET -ne 0 ] ; then
        echo_string ""
        echo_string "ERROR: Failed to create directory $MODUEL_INSTALL_DIR !"
        error_exit
    fi

    cp -rvf $HWS_TOP_DIR/src $MODUEL_INSTALL_DIR >> $LOGFILE 2>&1 &&
    cp -rvf $HWS_TOP_DIR/scripts $MODUEL_INSTALL_DIR >> $LOGFILE 2>&1
    RET=$?
    if [ $RET -ne 0 ] ; then
        echo_string ""
        echo_string "ERROR: Failed to copy driver files to $MODUEL_INSTALL_DIR !"
        error_exit
    fi
    ln -sf $MODUEL_INSTALL_DIR/scripts/hws-repair.sh /usr/bin/ >> $LOGFILE 2>&1
    ln -sf $MODUEL_INSTALL_DIR/scripts/hws-uninstall.sh /usr/bin/ >> $LOGFILE 2>&1

   
    $DEPMOD -a
    RET=$?
    if [ $RET -ne 0 ] ; then
        echo_string ""
        echo_string "ERROR: Failed to run $DEPMOD !"
        error_exit
    fi
    echo_string "Done."

}


SECOND=""
while getopts "s" flag ; do
   case "$flag" in
      s)SECOND="YES";;
   esac
done

if [ "YES" != "$SECOND" ] ; then
echo "==================================================="
echo "      HWS Linux Driver Installer"
echo "==================================================="
echo ""
fi

if [ `id -u` -ne 0 ] ; then
    sudo su -c "$0 -s $*"
    exit $?
fi

echo -n "" > $LOGFILE

echo_string_nonewline "Checking for required tools ... "
MISSING_TOOLS=""
REQUIRED_TOOLS="make gcc ld"
for tool in $REQUIRED_TOOLS ; do
    $tool --help > /dev/null 2>&1
    RET=$?
    if [ $RET -ne 0 ] ; then
        MISSING_TOOLS="$MISSING_TOOLS $tool"
    fi
done

if [ -n "$MISSING_TOOLS" ]; then
    echo_string ""
    echo_string ""
    echo_string "Your system has one or more system tools missing which are"
    echo_string "required to compile and load the HWS Linux driver."
    echo_string ""
    echo_string "Required tools: $MISSING_TOOLS"
    error_exit
else
    echo_string "Done."
fi


echo_string_nonewline "Checking for required packages ... "
KERNEL_BASE="/lib/modules/`uname -r`"
KERNEL_BUILD="$KERNEL_BASE/build"
if [ ! -d $KERNEL_BUILD ]; then
    echo_string ""
    echo_string ""
    echo_string "Your system is missing kernel development packages which"
    echo_string "is required to build and load the HWS Linux driver."
    echo_string ""
    echo_string "Required packages: kernel-devel"
    echo_string ""
    echo_string "Please make sure that the correct versions of these packages are"
    echo_string "installed.  Versions required: `uname -r`"
    error_exit
else
    echo_string "Done."
fi


echo_string_nonewline "Checking for previous installation ... "
KERNEL_STR=`uname -r`
MODULE_FILE=`find /lib/modules/$KERNEL_STR -iname "HwsUHDX1Capture.ko"`

if [ -n "$MODULE_FILE" -o -e "$MODUEL_INSTALL_DIR" ]; then
    echo_string "Found"
    echo_string "Removing previous installation..."

    if [ -f /usr/bin/hws-uninstall.sh ]; then
        /usr/bin/hws-uninstall.sh -n
    else
        $SCRIPT_PATH/hws-uninstall.sh -n
    fi
else
    echo_string "None"
fi


echo_string "Beginning install, please wait... "

DEPMOD=`which depmod 2>/dev/null`
if [ ! -e "$DEPMOD" ]; then
    echo_string ""
    echo_string "ERROR: Failed to find command: depmod"
    echo_string "   Please install depmod first!"
    echo_string ""
    error_exit
fi

build_prepare
build_module
install_module
build_clean


MODULE_LOADED=`lsmod | grep HwsCapture`
if [ -z "$MODULE_LOADED" ]; then
    MODPROBE=`which modprobe 2>/dev/null`
    if [ ! -e "$MODPROBE" ]; then
        echo_string "modprobe is not detected! Please load driver module manually!"
    else
        $MODPROBE HwsCapture
        RET=$?
        if [ $RET -ne 0 ] ; then
            echo_string "ERROR: Load driver module failed!"
            error_exit
        fi
    fi
    echo_string ""
    echo_string "========================================================"
    echo_string ""
    echo_string "Install Successfully!"
    echo_string "For more information please check the docs directory or"
    echo_string "contact alex.liu@longtimetech.com."
    echo_string ""
    echo_string "========================================================"
else
    echo_string ""
    echo_string "========================================================"
    echo_string ""
    echo_string "Install Successfully!"
    echo_string "For more information please check the docs directory or"
    echo_string "contact alex.liu@longtimetech.com."
    echo_string "!!!Previous installed module already loaded, reboot is needed! "
    echo_string_nonewline "Do you wish to reboot now (Y/N) [N]: "
    read cont

    if [ "$cont" != "YES" -a "$cont" != "yes" -a \
        "$cont" != "Y" -a "$cont" != "y" ] ; then
        echo_string "Reboot canceled! You should reboot your system manually later."
    else
        reboot
    fi

    echo_string ""
    echo_string "========================================================"
fi

echo_string ""

