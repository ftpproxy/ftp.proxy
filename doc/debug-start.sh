#!/bin/sh

echo >>/tmp/ftp.proxy.log
echo >>/tmp/ftp.proxy.log
date >>/tmp/ftp.proxy.log
echo >>/tmp/ftp.proxy.log

exec /usr/local/sbin/ftp.proxy -d -e -l 2>>/tmp/ftp.proxy.log

