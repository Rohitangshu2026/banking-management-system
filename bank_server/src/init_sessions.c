
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include "../include/common.h"

int main(void) {
    int fd_user, fd_session;
    User user;
    Session session;

    fd_user = open(USER_FILE, O_RDONLY);
    if (fd_user < 0) {
        perror("Failed to open users.txt");
        return 1;
    }

    fd_session = open(SESSION_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd_session < 0) {
        perror("Failed to create sessions.txt");
        close(fd_user);
        return 1;
    }

    int count = 0;

    while (read(fd_user, &user, sizeof(User)) == sizeof(User)) {
        memset(&session, 0, sizeof(Session));
        session.userId = user.id;
        session.fd = -1;
        session.pid = -1;

        if (write(fd_session, &session, sizeof(Session)) != sizeof(Session)) {
            perror("Write to sessions.txt failed");
            break;
        }
        count++;
    }

    close(fd_user);
    close(fd_session);

    char msg[128];
    snprintf(msg, sizeof(msg), "Successfully created %s with %d records.\n", SESSION_FILE, count);
    write(STDOUT_FILENO, msg, strlen(msg));

    return 0;
}

