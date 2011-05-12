#!/bin/sh
#

#
# Clear NAT table ...
#

iptables -F -t nat

#
# ... configure transparent redirection ...
#

iptables -t nat -A PREROUTING --protocol TCP \
	--dport 21 -j REDIRECT --to-port 2001

#
# ... and start ftp.proxy in daemon mode.
#

ftp.proxy -de -D 2001 -r forward-only


#
# Clean up.
#

iptables -F -t nat

