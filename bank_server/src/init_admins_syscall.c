#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include "../include/common.h"

int main() {
    int fd = open("../data/admins.txt", O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd < 0) {
        write(STDERR_FILENO, "Cannot open admin file\n", 23);
        _exit(1);
    }

    Admin a1 = {1, "Ayushi", "password123", 1};
    Admin a2 = {2, "Rohit", "password1234", 1};

    write(fd, &a1, sizeof(Admin));
    write(fd, &a2, sizeof(Admin));
    close(fd);

    write(STDOUT_FILENO, "Admin records created in binary format.\n", 40);
    return 0;
}
