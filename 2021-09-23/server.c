#include <linux/limits.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <string.h>

#ifndef PORT
#define PORT 10000
#endif

#define PATHMAX 1024
char protected_storage[PATHMAX] __attribute__((aligned(4096)));
char padding[4096-PATHMAX];

void set_permissions(int perm)
{
	if (mprotect(protected_storage, 4096, perm) < 0) {
		perror("mprotect");
		exit(1);
	}
}

void fillbuf(char *buf)
{
	int rem, n;

	rem = PATH_MAX;
	while (rem > 0 && (n = read(0, buf, rem)) > 0) {
		buf += n;
		rem -= n;
	}
}

void child()
{
	char buf[PATHMAX], cmd;
	int n;

	set_permissions(PROT_NONE);
	for (;;) {
		if (read(0, &cmd, 1) < 1)
			break;
		switch (cmd) {
		case 'w':
			fillbuf(buf);
			set_permissions(PROT_WRITE);
			memcpy(protected_storage, buf, PATHMAX);
			set_permissions(PROT_NONE);
			break;
		case 'r':
			set_permissions(PROT_READ);
			write(1, protected_storage, PATHMAX);
			set_permissions(PROT_NONE);
			break;
		case 'q':
			return;
		default:
			fprintf(stderr, "unknown command\n");
			break;
		}
	}
}

void sigchld(int signo)
{
	int status;
	pid_t pid = waitpid(-1, &status, WNOHANG);
	if (pid > 0) {
		printf("child %ld: ", (long)pid);
		if (WIFEXITED(status)) {
			printf("exited with status: %d\n", WEXITSTATUS(status));
		} else {
			printf("%s\n", strsignal(WTERMSIG(status)));
		}
	}
}

int main()
{
	int lstn;
	int enable;
	struct sockaddr_in lstn_addr;

	lstn = socket(AF_INET, SOCK_STREAM, 0);
	if (lstn < 0) {
		perror("socket");
		return 1;
	}
	enable = 1;
	if (setsockopt(lstn, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) < 0) {
		perror("setsockopt");
		return 1;
	}
	bzero(&lstn_addr, sizeof(lstn_addr));

	lstn_addr.sin_family = AF_INET;
	lstn_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	lstn_addr.sin_port = htons(PORT);

	if (bind(lstn, (struct sockaddr *)&lstn_addr, sizeof(lstn_addr)) < 0) {
		perror("bind");
		return 1;
	}

	if (listen(lstn, 10) < 0) {
		perror("listen");
		return 1;
	}
	printf("Listening on port %d\n", PORT);

	signal(SIGCHLD, sigchld);

	for (;;) {
		int con = accept(lstn, NULL, NULL);
		if (con < 0) {
			perror("accept");
			return 1;
		}

		switch (fork()) {
		case -1:
			perror("fork");
			return 1;
		case 0:
			printf("New connection, child %d\n", getpid());

			close(0);
			dup(con);
			close(1);
			dup(con);
			close(2);
			dup(con);
			child();
			exit(0);
			break;
		default:
			close(con);
			break;
		}
	}
	return 0;
}
