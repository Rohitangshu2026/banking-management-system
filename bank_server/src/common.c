#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <ctype.h>
#include "../include/common.h"
#include <fcntl.h>
ssize_t readLine(int sock, char *buf, size_t size) {
    if (size == 0) return -1;
    ssize_t i = 0;
    char ch;
    while (i < (ssize_t)(size - 1)) {
        ssize_t n = read(sock, &ch, 1);
        if (n <= 0) break;
        if (ch == '\n' || ch == '\r') break;
        buf[i++] = ch;
    }
    buf[i] = '\0';
    return i;
}

int isUserActive(int userId) {
    int fd = open(USER_FILE, O_RDONLY);
    if (fd < 0) {
        perror("isUserActive: open USER_FILE");
        return 0; 
    }

    User user;
    int isActive = 0;
    while(read(fd, &user, sizeof(User)) == sizeof(User)) {
        if (user.id == userId) {
            if (user.isActive == 1) {
                isActive = 1;
            }
            break;
        }
    }
    close(fd);
    return isActive;
}

int checkUserRole(int userId, const char* expectedRole) {
    int fd = open(USER_FILE, O_RDONLY);
    if (fd < 0) return 0;

    User user;
    int hasRole = 0;
    while(read(fd, &user, sizeof(User)) == sizeof(User)) {
        if (user.id == userId) {
            if (strcmp(user.role, expectedRole) == 0) {
                hasRole = 1;
            }
            break;
        }
    }
    close(fd);
    return hasRole;
}

int parse_int_strict(const char *s, int *out) {
    if (!s || !out || *s == '\0') return 0;

    /* Reject leading whitespace — menu input shouldn't have any after
     * readLine has stripped the newline, and accepting it hides
     * client bugs. */
    if (isspace((unsigned char)*s)) return 0;

    errno = 0;
    char *end = NULL;
    long v = strtol(s, &end, 10);

    /* Whole string must be consumed. */
    if (end == s || *end != '\0') return 0;
    if (errno == ERANGE || v > INT_MAX || v < INT_MIN) return 0;

    *out = (int)v;
    return 1;
}

int safe_strcpy(char *dst, const char *src, size_t dst_sz) {
    if (!dst || dst_sz == 0) return 0;
    if (!src) { dst[0] = '\0'; return 0; }

    size_t src_len = strlen(src);
    size_t copy = (src_len < dst_sz - 1) ? src_len : dst_sz - 1;
    memcpy(dst, src, copy);
    dst[copy] = '\0';
    return src_len < dst_sz;
}