#!/bin/sh
#

#
# operator.sh - Simple wrapper around our configuration operator.
#

dir=`dirname $0`
exec $dir/operator.ctp $dir/proxy-user.conf
exit 1

