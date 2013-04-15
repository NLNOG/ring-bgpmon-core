#!/bin/sh
#
# $FREEBSD$
#
# PROVIDE: bgpmon
# REQUIRE: DAEMON
#
# Author: Mikhail Strizhov
# strizhov@cs.colostate.edu
#


. /etc/rc.subr

name="bgpmon"
rcvar=${name}_enable

command="/usr/local/bin/bgpmon"
command_args="-d -c /usr/local/etc/bgpmon_config.txt -s"
pidfile="/var/run/${name}.pid"

load_rc_config $name

: ${bgpmon_enable="NO"}

run_rc_command "$1"

