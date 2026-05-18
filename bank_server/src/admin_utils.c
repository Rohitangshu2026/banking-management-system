#include "../include/common.h"
#include "../include/admin_utils.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>


#define RECORD_SIZE sizeof(Admin)

void lock_record(int fd, off_t offset, short lock_type) {
    struct flock lock = {0};
    lock.l_type = lock_type;
    lock.l_whence = SEEK_SET;
    lock.l_start = offset;
    lock.l_len = RECORD_SIZE;
    lock.l_pid = getpid();

    if (fcntl(fd, F_SETLKW, &lock) == -1) {
        perror("Error locking record");
        _exit(1);
    }
}

void unlock_record(int fd, off_t offset) {
    struct flock lock = {0};
    lock.l_type = F_UNLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = offset;
    lock.l_len = RECORD_SIZE;
    lock.l_pid = getpid();

    if (fcntl(fd, F_SETLK, &lock) == -1)
        perror("Error unlocking record");
}

int validateAdmin(const char *username, const char *password, UserLockInfo *lockInfo) {
    int fd = open(ADMIN_FILE, O_RDWR);
    if (fd < 0) {
        perror("Error opening admin file");
        return 0;
    }

    Admin admin;
    off_t offset = 0;

    while (read(fd, &admin, sizeof(Admin)) == sizeof(Admin)) {
        if (strcmp(admin.username, username) == 0) {
            if (strcmp(admin.password, password) != 0) {
                close(fd);
                return 0;
            }

            struct flock lock = {0};
            lock.l_type = F_WRLCK;
            lock.l_whence = SEEK_SET;
            lock.l_start = offset;
            lock.l_len = sizeof(Admin);

            if (fcntl(fd, F_SETLK, &lock) == -1) {
                if (errno == EACCES || errno == EAGAIN) {
                    close(fd);
                    return -2; 
                }
                perror("Lock error");
                close(fd);
                return 0;
            }
            lockInfo->fd = fd;
            lockInfo->start = offset;
            lockInfo->len = sizeof(Admin);

            return 1;
        }
        offset += sizeof(Admin);
    }

    close(fd);
    return 0;
}


void unlockAdmin(UserLockInfo *lockInfo) {
    if (!lockInfo || lockInfo->fd < 0) return;

    struct flock lock = {0};
    lock.l_type = F_UNLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = lockInfo->start;
    lock.l_len = lockInfo->len;

    fcntl(lockInfo->fd, F_SETLK, &lock);
    close(lockInfo->fd);
    lockInfo->fd = -1;
}


void addEmployee(int sock) {
    User user;
    int fd_user;
    struct flock lock;
    ssize_t n;
    char buffer[128];
    int role_choice;

    memset(&user, 0, sizeof(User));

    write(sock, "Enter Employee Username: ", 25);
    n = read(sock, user.username, sizeof(user.username));
    if (n <= 0) return;
    user.username[strcspn(user.username, "\r\n")] = '\0';

    const char *menu =
        "\nSelect Employee Role:\n"
        "1. Manager\n"
        "2. Employee\n"
        "Enter choice (1-2): ";
    write(sock, menu, strlen(menu));

    n = read(sock, buffer, sizeof(buffer));
    if (n <= 0) return;
    buffer[strcspn(buffer, "\r\n")] = '\0';
    role_choice = atoi(buffer);

    if (role_choice == 1)
        strcpy(user.role, "manager");
    else if (role_choice == 2)
        strcpy(user.role, "employee");
    else {
        write(sock, "Invalid role choice.\n", 21);
        return;
    }

    write(sock, "Enter Password: ", 15);
    n = read(sock, user.password, sizeof(user.password));
    if (n <= 0) return;
    user.password[strcspn(user.password, "\r\n")] = '\0';
    user.isActive = 1;

    fd_user = open(USER_FILE, O_RDWR | O_CREAT, 0600);
    if (fd_user < 0) {
        write(sock, "Error: could not open users.txt\n", 32);
        return;
    }

    memset(&lock, 0, sizeof(lock));
    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;
    if (fcntl(fd_user, F_SETLKW, &lock) == -1) {
        write(sock, "Could not lock users.txt\n", 25);
        close(fd_user);
        return;
    }

    User temp;
    int last_id = 0;
    lseek(fd_user, 0, SEEK_SET);
    while (read(fd_user, &temp, sizeof(User)) == sizeof(User)) {
        if (temp.id > last_id)
            last_id = temp.id;
    }
    user.id = last_id + 1;

    lseek(fd_user, 0, SEEK_END);
    if (write(fd_user, &user, sizeof(User)) != sizeof(User)) {
        write(sock, "Error writing user record.\n", 27);
    } else {
        char msg[128];
        snprintf(msg, sizeof(msg),
                 "Employee added successfully. Employee ID: %d\n", user.id);
        write(sock, msg, strlen(msg));
    }

    lock.l_type = F_UNLCK;
    fcntl(fd_user, F_SETLK, &lock);
    close(fd_user);
}


void viewLogs(int sock) {
    int fd = open(LOG_FILE, O_RDONLY | O_CREAT, 0600);
    if (fd < 0) {
        write(sock, "Error opening log file\n", 23);
        return;
    }

    off_t size = lseek(fd, 0, SEEK_END);
    if (size == 0) {
        write(sock, "No logs found.\n", 15);
        close(fd);
        return;
    }
    lseek(fd, 0, SEEK_SET);

    char buffer[256];
    ssize_t bytes;
    while ((bytes = read(fd, buffer, sizeof(buffer))) > 0)
        write(sock, buffer, bytes);

    close(fd);
}

#include "../include/common.h"
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

void modifyUser(int sock) {
    char targetUsername[64];
    write(sock, "Enter username to modify: ", 26);
    if (readLine(sock, targetUsername, sizeof(targetUsername)) <= 0)
        return;

    int fd = open(USER_FILE, O_RDWR);
    if (fd < 0) {
        write(sock, "Error opening user file.\n", 25);
        return;
    }

    User user;
    off_t offset = 0;
    int found = 0;

    while (read(fd, &user, sizeof(User)) == sizeof(User)) {
        if (strcmp(user.username, targetUsername) == 0) {
            found = 1;
            break;
        }
        offset += sizeof(User);
    }

    if (!found) {
        write(sock, "User not found.\n", 16);
        close(fd);
        return;
    }

    struct flock lock = {0};
    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = offset;
    lock.l_len = sizeof(User);

    if (fcntl(fd, F_SETLK, &lock) == -1) {
        write(sock, "Record locked by another admin. Try again later.\n", 49);
        close(fd);
        return;
    }

    char newUsername[64], newPassword[64];

    snprintf(newUsername, sizeof(newUsername), "%s", user.username);
    snprintf(newPassword, sizeof(newPassword), "%s", user.password);

    write(sock, "Enter new username (press Enter to keep same): ", 48);
    if (readLine(sock, newUsername, sizeof(newUsername)) <= 0 || strlen(newUsername) == 0)
        strcpy(newUsername, user.username);

    write(sock, "Enter new password (press Enter to keep same): ", 48);
    if (readLine(sock, newPassword, sizeof(newPassword)) <= 0 || strlen(newPassword) == 0)
        strcpy(newPassword, user.password);

    strncpy(user.username, newUsername, sizeof(user.username) - 1);
    strncpy(user.password, newPassword, sizeof(user.password) - 1);
    user.username[sizeof(user.username) - 1] = '\0';
    user.password[sizeof(user.password) - 1] = '\0';

    lseek(fd, offset, SEEK_SET);
    if (write(fd, &user, sizeof(User)) != sizeof(User)) {
        write(sock, "Failed to update record.\n", 25);
    } else {
        write(sock, "User details modified successfully.\n", 36);

        int log_fd = open(LOG_FILE, O_WRONLY | O_APPEND | O_CREAT, 0600);
        if (log_fd >= 0) {
            dprintf(log_fd, "Modified user: %s → %s\n", targetUsername, user.username);
            close(log_fd);
        }
    }

    lock.l_type = F_UNLCK;
    fcntl(fd, F_SETLK, &lock);
    close(fd);
}


void changeAdminPassword(int sock, Admin *admin, UserLockInfo *lockInfo) {
    char newPass[64], confirmPass[64];
    Admin a;

    write(sock, "Enter new password: ", 20);
    if (readLine(sock, newPass, sizeof(newPass)) <= 0) return;

    write(sock, "Confirm new password: ", 22);
    if (readLine(sock, confirmPass, sizeof(confirmPass)) <= 0) return;

    if (strcmp(newPass, confirmPass) != 0) {
        write(sock, "Passwords do not match.\n", 24);
        return;
    }

    int fd = lockInfo->fd;
    if (fd < 0) {
        write(sock, "Invalid admin file descriptor.\n", 30);
        return;
    }

    lseek(fd, lockInfo->start, SEEK_SET);
    if (read(fd, &a, sizeof(Admin)) != sizeof(Admin)) {
        write(sock, "Error reading admin record.\n", 28);
        return;
    }

    strncpy(a.password, newPass, sizeof(a.password) - 1);
    a.password[sizeof(a.password) - 1] = '\0';

    lseek(fd, lockInfo->start, SEEK_SET);
    ssize_t w = write(fd, &a, sizeof(Admin));
    if (w != sizeof(Admin)) {
        write(sock, "Failed to update password.\n", 27);
        return;
    }

    fsync(fd);
    write(sock, "Password updated successfully.\n", 31);
}

static void unassignEmployeeLoans(int sock, int demotedEmployeeId) {
    int fd_loan = open(LOAN_FILE, O_RDWR);
    if (fd_loan < 0) {
        return;
    }

    struct flock lock = {0};
    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;
    
    if (fcntl(fd_loan, F_SETLKW, &lock) == -1) {
        write(sock, "Could not lock loan file to unassign tasks.\n", 44);
        close(fd_loan);
        return;
    }

    Loan loan;
    off_t offset = 0;
    int count = 0;

    while(read(fd_loan, &loan, sizeof(Loan)) == sizeof(Loan)) {
        if (loan.employeeId == demotedEmployeeId && strcmp(loan.status, "PENDING") == 0) {
            loan.employeeId = -1; 

            lseek(fd_loan, offset, SEEK_SET);
            if (write(fd_loan, &loan, sizeof(Loan)) != sizeof(Loan)) {
                write(sock, "Error unassigning a loan record.\n", 33);
            }
            fsync(fd_loan);
            count++;
        }
        offset += sizeof(Loan);
    }
    
    if (count > 0) {
        char buffer[128];
        snprintf(buffer, sizeof(buffer), "%d pending loan(s) were unassigned from the user.\n", count);
        write(sock, buffer, strlen(buffer));
    }

    lock.l_type = F_UNLCK;
    fcntl(fd_loan, F_SETLK, &lock);
    close(fd_loan);
}

void manageUserRoles(int sock) {
    int fd;
    struct flock lock;
    User user;
    char uname[64], choice[8], msg[128];
    int found = 0;
    int original_user_id = 0;
    char original_role[MAX_ROLE];

    write(sock, "Enter username to change role: ", 31);
    memset(uname, 0, sizeof(uname));
    if (readLine(sock, uname, sizeof(uname)) <= 0)
        return;
    
    fd = open(USER_FILE, O_RDWR);
    if (fd < 0) {
        write(sock, "Cannot open users.txt\n", 22);
        return;
    }

    off_t offset = 0;
    while (read(fd, &user, sizeof(User)) == sizeof(User)) {
        if (strcmp(user.username, uname) == 0) {
            found = 1;
            original_user_id = user.id; 
            strcpy(original_role, user.role);

            memset(&lock, 0, sizeof(lock));
            lock.l_type = F_WRLCK;
            lock.l_whence = SEEK_SET;
            lock.l_start = offset;
            lock.l_len = sizeof(User);

            if (fcntl(fd, F_SETLKW, &lock) == -1) {
                write(sock, "Record lock failed. Try again.\n", 31);
                close(fd);
                return;
            }
            
            if (strcmp(user.role, "customer") == 0) {
                 write(sock, "Cannot change a customer's role here.\n", 37);
                 goto unlock_and_close; 
            }

            write(sock, "\nSelect new role:\n1. Employee\n2. Manager\n> ", 43);
            memset(choice, 0, sizeof(choice));
            if (readLine(sock, choice, sizeof(choice)) <= 0)
                goto unlock_and_close;

            int opt = atoi(choice);
            char new_role[32];
            if (opt == 1)
                strcpy(new_role, "employee");
            else if (opt == 2)
                strcpy(new_role, "manager");
            else {
                write(sock, "Invalid choice.\n", 16);
                goto unlock_and_close;
            }

            if (strcmp(user.role, new_role) == 0) {
                write(sock, "No change in role.\n", 19);
                goto unlock_and_close;
            }
            strncpy(user.role, new_role, sizeof(user.role) - 1);
            user.role[sizeof(user.role) - 1] = '\0';

            lseek(fd, offset, SEEK_SET);
            if (write(fd, &user, sizeof(User)) != sizeof(User)) {
                write(sock, "Error updating user record.\n", 28);
                goto unlock_and_close;
            }

            if (strcmp(original_role, "employee") == 0) {
                unassignEmployeeLoans(sock, original_user_id);
            }
            int log_fd = open(LOG_FILE, O_WRONLY | O_APPEND | O_CREAT, 0600);
            if (log_fd >= 0) {
                dprintf(log_fd, "Role changed: %s (%s → %s)\n",
                        user.username, original_role, new_role);
                close(log_fd);
            }

            snprintf(msg, sizeof(msg), "Role updated: %s → %s\n",
                     original_role, new_role);
            write(sock, msg, strlen(msg));
            goto unlock_and_close;
        }
        offset += sizeof(User);
    }

    if (!found)
        write(sock, "Username not found.\n", 20);

unlock_and_close:
    if (found) {
        lock.l_type = F_UNLCK;
        fcntl(fd, F_SETLK, &lock);
    }
    close(fd);
}