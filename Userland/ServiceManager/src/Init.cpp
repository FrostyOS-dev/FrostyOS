/*
Copyright (©) 2026  Frosty515

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <asm/ioctls.h>

#include <frostyos/syscall.h>

#include <sys/ioctl.h>
#include <sys/wait.h>

int dbgprintln(const char* msg) {
    return syscall(SYSCALL_DBGPRINTLN, (uint64_t)msg, strlen(msg));
}

#define ERROR(msg) do { dbgprintln(msg); abort(); } while (0)
#define ERROR2(msg) do { perror(msg); abort(); } while (0)

int main(int argc, char** argv) {
    if (argc < 1 || strcmp(argv[0], "init") != 0 || getpid() != 1) {
        fprintf(stderr, "ServiceManager must not be called directly\n");
        return 1;
    }

    int fd = open("/dev/tty0", O_RDWR | O_NOCTTY);
    if (fd < 0)
        ERROR("Open of console failed!");

    int rc;

    for (int i = 0; i < 3; i++) {
        if (fd != i) {
            rc = dup2(fd, i);
            if (rc < 0)
                break;
        }
    }

    if (fd > 2)
        close(fd);

    if (rc < 0)
        ERROR("dup2 failed");

    pid_t pid = fork();
    if (pid < 0)
        ERROR2("fork");

    if (pid == 0) {
        // child
        setsid();

        ioctl(STDIN_FILENO, TIOCSCTTY, 0);

        if (chdir("/root") < 0)
            ERROR2("chdir");

        char const* argv[] = { "bash", "--login", NULL };
        char const* env[] = { "PATH=/data/bin", "LD_LIBRARY_PATH=/data/lib", "HOME=/root", "TERM=linux", NULL };
        rc = execve("/data/bin/bash", (char**)argv, (char**)env);
        ERROR2("execve");
    } else {
        while (true) {
            int status;
            pid_t child = waitpid(-1, &status, 0);
            if (child < 0)
                continue;
            fprintf(stderr, "Child %d exited with code %d", child, WEXITSTATUS(status));
        }
    }
}