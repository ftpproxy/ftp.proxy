#!/usr/local/bin/perl -w
#
#	Sample acp for authorizing proxy user against system.
#
#
#	Copyright (C) 2005 Omkumar Sheshadri <omkumar@ti.com>
#
#	This software is free software; you can redistribute it and/or modify
#	it under the terms of the GNU General Public License as published by
#	the Free Software Foundation; either version 2 of the License, or
#	(at your option) any later version.
#
# 	This program is distributed in the hope that it will be useful,
#	but WITHOUT ANY WARRANTY; without even the implied warranty of
#	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#	GNU General Public License for more details.
#
#	You should have received a copy of the GNU General Public License
#	along with this program; if not, write to the Free Software
#	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#


$ENV{PATH} = "/bin:/usr/bin:/sbin:/usr/sbin";


if(!defined($ENV{PROXY_USERNAME}) or !defined($ENV{PROXY_PASSWD})) {
	print STDERR "Please set the environment variables PROXY_USERNAME & PROXY_PASSWD\n";
	exit(1);
}

$PROXY_USERNAME = $ENV{PROXY_USERNAME};
$PROXY_PASSWD   = $ENV{PROXY_PASSWD};

@pass = getpwnam("$PROXY_USERNAME");
if(!defined($pass[0])) {
	print STDERR "Authentication failure...\n";
	exit(1);
}

$passwd = $pass[1];
$salt   = substr($passwd,0,2);
$mypass = crypt($PROXY_PASSWD,$salt);

$LOG = "/var/log/FTP_PROXY_LOG";
open(LKFD,">>$LOG") || die "Cannot open $LOG: $!";
flock(LKFD,2) || die "Cannot lock: $!";
seek(LKFD,0,2);
$date = `date`;
chomp($date);

$ts = time();
$date = `date "+%h-%d-%H:%M:%S"`;
chomp($date);

if($mypass ne $passwd) {
	print STDERR "Authentication failure...\n";
	print LKFD "$ts $date Authentication Failure For User ($PROXY_USERNAME)\n";
	close(LKFD);
	exit(1);
}

print STDERR "Success....\n";
print LKFD "$ts $date Authentication Succeeded For User ($PROXY_USERNAME)\n";
close(LKFD);
exit(0);
