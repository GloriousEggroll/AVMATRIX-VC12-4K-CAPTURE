#!/bin/sh

LOGFILE=hws_install.log

SCRIPT_PATH="$(cd "$(dirname "$0")" && pwd)"
HWS_TOP_DIR=$SCRIPT_PATH/..
SRC_DIR=$HWS_TOP_DIR/src
MODULE_NAME=HwsUHDX1Capture
MODULE_INSTALL_DIR=/usr/local/share/HWS

echo_string ()
{
    echo "$1" | tee -a $LOGFILE
}

echo_string_nonewline ()
{
    echo -n "$1" | tee -a $LOGFILE
}

NO_REBOOT_PROMPT=""
while getopts "n" flag ; do
   case "$flag" in
      n)NO_REBOOT_PROMPT="YES";;
   esac
done

if [ `id -u` -ne 0 ] ; then
    sudo su -c "$0 $*"
    exit $?
fi

DEPMOD=`which depmod 2>/dev/null`
if [ ! -e "$DEPMOD" ]; then
    echo_string ""
    echo_string "ERROR: Failed to find command: depmod"
    echo_string "   Please install depmod first!"
    echo_string ""
    exit
fi

remove_module ()
{
    KERNEL_VERSION=`uname -r`
    MODULE_PATH=`find /lib/modules/$KERNEL_VERSION -name ${MODULE_NAME}.ko`
    echo_string_nonewline "Removing $MODULE_PATH ... "
    rm -vf $MODULE_PATH >> $LOGFILE 2>&1
    RET=$?
    if [ $RET -ne 0 ] ; then
        echo_string "Remove $MODULE_PATH failed!"
        exit
    fi
    echo_string "Done."

    if [ -e $DEPMOD ] ; then
        echo_string_nonewline "Re-generating modules.dep and map files ... "
        $DEPMOD -a >> $LOGFILE 2>&1
        echo_string "Done."
    fi

    if [ -d $MODULE_INSTALL_DIR ]; then
        echo_string_nonewline "Removing installed files ... "
        rm -vrf $MODULE_INSTALL_DIR >> $LOGFILE 2>&1
        echo_string "Done."
    fi

    if [ -e /usr/bin/hws-repair.sh ]; then
        rm -vf /usr/bin/hws-repair.sh >> $LOGFILE 2>&1
    fi

    if [ -e /usr/bin/hws-uninstall.sh ]; then
        rm -vf /usr/bin/hws-uninstall.sh >> $LOGFILE 2>&1
    fi
}

remove_module

MODULE_LOADED=`lsmod | grep HwsUHDX1Capture`
echo_string ""
if [ -z "$MODULE_LOADED" -o x"$NO_REBOOT_PROMPT" = x"YES" ]; then
echo_string "Uninstall Successfully!"
else
    echo_string "Uninstall Successfully!"
    echo_string "!!!!Reboot is needed to unload module!"
    echo_string_nonewline "Do you wish to reboot now (Y/N) [N]: "
    read cont

    if [ "$cont" != "YES" -a "$cont" != "yes" -a \
        "$cont" != "Y" -a "$cont" != "y" ] ; then
        echo_string "Reboot canceled! You should reboot your system manually later."
    else
        reboot
    fi
fi
echo_string ""

