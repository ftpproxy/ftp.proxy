#!/usr/bin/awk -f
#


function setval(var, val) {
	if (val == "")
		return (0);

	conf[var] = val;
	return (0);
	}

function setconfig(line,   n, x) {
	n = split(line, x, ":");

	#
	# If the record has only two field it denies access.
	#
	if (n == 2) {
		printf ("-ERR: %s: request denied by configuration: %s/%s\n",
				program, ENVIRON["FTP_USER"], ENVIRON["FTP_SERVER"]);
		exit (1);		# signal error anyway.
		}

	setval("server", x[3]);
	setval("login", x[4]);
	setval("passwd", x[5]);

	return (0);
	}


BEGIN {
	program = "proxy-operator";
	STDERR = "/dev/stderr";

	argi = 1;
	if (argi >= ARGC)
		config = "/etc/proxy-user.conf"
	else {
		config = ARGV[argi];
		ARGV[argi++] = "";
		}

	lines = 0;
	while (getline line <config > 0) {
		lines++;
		sub(/[ \t]*#.*$/, "", line);	# Remove comments ...
		sub(/[ \t]+$/, "", line);	# ... and white-space at the end of the line.
		if (line == "")
			continue;		# Skip over blanks lines.


		#
		# Here we store our configuration lines `as-is' in the
		# `rec' array using incoming username and server as key.
		#

		n = split(line, x, ":");
		rec[x[1], x[2]] = line;
		}

	close (config);

	#
	# Let's check if we read something from the configuration
	# file.  If not it's either completly empty or it does not
	# exist.  We don't know (and we can't determine) but we
	# assume that is does not exist.
	#

	if (lines == 0) {
		printf ("%s: missing or empty configuration file\n", program) >STDERR;
		exit (1);
		}

	username = ENVIRON["FTP_USER"];
	server = ENVIRON["FTP_SERVER"];

	if ((username SUBSEP server) in rec)
		setconfig(rec[username, server]);

	#
	# An open question: If we don't have a record for a given
	# user/server request but wildcards for user and server, which
	# should be checked first (and take precedence over the other)?
	#

	else if ((username SUBSEP "*") in rec)
		setconfig(rec[username, "*"]);
	else if (("*" SUBSEP server) in rec)
		setconfig(rec["*", server]);
	else if (("*" SUBSEP "*") in rec)
		setconfig(rec["*", "*"]);

	for (var in conf)
		printf ("%s %s\n", toupper(var), conf[var]);

	exit (0);
	}

