#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <fcntl.h> 

#include "../include/common.h"
#include "../include/manager.h"
#include "../include/manager_utils.h"

int managerMenu(int sock) {
    User currentManager;
    memset(&currentManager, 0, sizeof(User));

    char buf[256], choiceBuf[32];
    int session_fd = -1;

    write(sock, "Enter username: ", 16);
    if (readLine(sock, currentManager.username, sizeof(currentManager.username)) <= 0) return -1;

    write(sock, "Enter password: ", 16);
    if (readLine(sock, currentManager.password, sizeof(currentManager.password)) <= 0) return -1;

    int result = validateManager(currentManager.username, currentManager.password);

    if (result == -2) {
        write(sock, "Manager already logged in from another terminal.\n", 52);
        return -1;
    } else if (result <= 0) { 
        write(sock, "Invalid credentials.\n", 22);
        return -1;
    }

    session_fd = result;

    int fd_user = open(USER_FILE, O_RDONLY);
    if (fd_user >= 0) {
        User u;
        while(read(fd_user, &u, sizeof(User)) == sizeof(User)) {
            if (strcmp(u.username, currentManager.username) == 0) {
                currentManager.id = u.id; 
                break;
            }
        }
        close(fd_user);
    }
    
    write(sock, "Login successful!\n", 18);

    while (1) {
        snprintf(buf, sizeof(buf),
                 "\n===== MANAGER MENU =====\n"
                 "1. Activate/Deactivate Customer Accounts\n"
                 "2. Assign Loan Applications to Employees\n"
                 "3. Review Customer Feedback\n"
                 "4. Change Password\n"
                 "5. Logout\n"
                 "Enter your choice: ");
        write(sock, buf, strlen(buf));

        memset(choiceBuf, 0, sizeof(choiceBuf));
        
        if (readLine(sock, choiceBuf, sizeof(choiceBuf) - 1) <= 0) {
            logoutManager(session_fd); 
            return -1;
        }

        choiceBuf[strcspn(choiceBuf, "\n")] = '\0';
        int choice;
        if (!parse_int_strict(choiceBuf, &choice)) {
            write(sock, "Invalid choice. Please enter a number.\n", 39);
            continue;
        }
        
        int op_status = 1;

        switch (choice) {
            case 1:
                op_status = toggleCustomerStatus(sock, currentManager.id);
                break;
            case 2:
                op_status = assignLoanToEmployee(sock, currentManager.id);
                break;
            case 3:
                op_status = reviewCustomerFeedback(sock, currentManager.id);
                break;
            case 4:
                op_status = changeManagerPassword(sock, currentManager.id);
                break;
            case 5:
                logoutManager(session_fd);
                write(sock, "Logged out successfully.\n", 27);
                return -1; 
            default:
                write(sock, "Invalid choice.\n", 17);
                break;
        }

        if (op_status == 0) {
            logoutManager(session_fd);
            return -1; 
        }
    }
    return -1; 
}