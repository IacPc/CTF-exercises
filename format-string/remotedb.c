#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <syslog.h>

#include "server.h"

#define BUFSZ 1024

/* send an error message to the client */
static void error(const char *err, const char *cmd);

static void search(const char *logfile, const char *arg)
{
	execlp("grep", "grep", "-E", arg, logfile, NULL);
	error("cannot exec egrep", NULL);
}

static void do_find(const char *cmd)
{
	int n = strlen(cmd);
	int i;
	char *buf, *arg;
	int inword, nwords;

	for (i = 0; i < n; i++) {
		if (!isalpha(cmd[i]) && !isspace(cmd[i])) {
			error("non-alphanumeric characters not allowed", NULL);
			return;
		}
	}

	arg = malloc(n + 3);
	if (arg == NULL) {
		error("out of memory", NULL);
		return;
	}
	buf = arg;
	inword = 0;
	nwords = 0;
	*buf++ = '(';
	while (*cmd != '\0') {
		if (isalpha(*cmd)) {
			if (!inword) {
				inword = 1;
				if (nwords)
					*buf++ = '|';
			}
			*buf++ = *cmd++;
		} else if (isspace(*cmd)) {
			if (inword)
				nwords++;
			inword = 0;
			cmd++;
		}
	}
	*buf++ = ')';
	*buf++ = '\0';
	if (verbosity > 1)
		msg("regexp: '%s'\n", arg);

	search(dbfile, arg);
	free(arg);
}

static void do_status()
{
	FILE *f;
	int n;
	char team1, team2, service;

	f = fopen(licencefile, "r");
	if (f == NULL) {
		printf("no license found!\n");
		goto out;
	}
	n = fscanf(f, "Team%c%c_Service%c", &team1, &team2, &service);
	if (n != 3 || !isdigit(team1) || !isdigit(team2) || !isdigit(service)) {
		printf("malformed licence!\n");
		goto out_close;
	}

	printf("OK\n");

out_close:
	fclose(f);
out:
	fflush(stdout);
}

void child()
{
	char buf[BUFSZ];

	printf("Available commands:\n");
	printf(" STS                  check the license\n");
	printf(" FND word1 word2 ...  print the db lines that contain any of the words\n");
	printf("                      (one search for connection).\n");
	fflush(stdout);

	while (fgets(buf, BUFSZ, stdin)) {
		size_t n = strlen(buf);

		if (verbosity > 1)
			msg("received: %s", buf);

		if (n < 4) {
			error("too short", NULL);
			continue;
		}

		if (strncmp(buf, "FND ", 4) == 0)
			do_find(buf + 4);
		else if (strncmp(buf, "STS", 3) == 0)
			do_status();
		else error("unknown: ", buf);
	}
}

void error(const char *err, const char *cmd)
{
	fprintf(stderr, err);
	fprintf(stderr, cmd == NULL ? "\n" : cmd);
}


// flag.txt 