#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

#include "../include/common.h"
#include "../include/customer.h"
#include "../include/customer_utils.h"

int customerMenu(int sock) {
    Customer currentCustomer;
    memset(&currentCustomer, 0, sizeof(Customer));

    char username[MAX_NAME];
    char password[MAX_PASS];
    char buffer[128];
    int session_fd = -1; 

    write(sock, "Enter username: ", 16);
    if (readLine(sock, username, sizeof(username)) <= 0) return -1;

    write(sock, "Enter password: ", 16);
    if (readLine(sock, password, sizeof(password)) <= 0) return -1;

    int result = validateCustomer(username, password, &currentCustomer);

    if (result == -2) {
        write(sock, "Customer account is already logged in.\n", 40);
        return -1;
    } else if (result <= 0) { 
        write(sock, "Invalid credentials or account is inactive.\n", 45);
        return -1;
    }

    session_fd = result;
    write(sock, "Login successful!\n", 18);

    /* Emit the account id so out-of-band consumers (the HTTP gateway)
     * can record it without having to call viewBalance separately. The
     * CLI client just prints this line through to the terminal — no
     * harm, and a useful breadcrumb for the user. */
    char acctLine[64];
    int acctLen = snprintf(acctLine, sizeof(acctLine),
                           "Account: %d\n", currentCustomer.id);
    if (acctLen > 0) {
        write(sock, acctLine, (size_t)acctLen);
    }

    int running = 1;
    while (running) {
        const char *menu =
            "\n===== CUSTOMER MENU =====\n"
            "1. View Balance\n"
            "2. Deposit Money\n"
            "3. Withdraw Money\n"
            "4. Transfer Funds\n"
            "5. View Transaction History\n"
            "6. Apply for Loan\n"
            "7. Change Password\n"
            "8. Add Feedback\n"   
            "9. Logout\n"        
            "Enter your choice: ";

        write(sock, menu, strlen(menu));

        memset(buffer, 0, sizeof(buffer));

        if (readLine(sock, buffer, sizeof(buffer)) <= 0) {
            logoutCustomer(session_fd);
            break;
        }

        int choice;
        if (!parse_int_strict(buffer, &choice)) {
            write(sock, "Invalid choice. Please enter a number.\n", 39);
            continue;
        }

        int op_status = 1;

        switch (choice) {
            case 1:
                op_status = viewBalance(sock, currentCustomer.id, currentCustomer.userId);
                break;
            case 2:
                op_status = depositMoney(sock, currentCustomer.id, currentCustomer.userId);
                break;
            case 3:
                op_status = withdrawMoney(sock, currentCustomer.id, currentCustomer.userId);
                break;
            case 4:
                op_status = transferFunds(sock, currentCustomer.id, currentCustomer.userId);
                break;
            case 5:
                op_status = viewTransactionHistory(sock, currentCustomer.id, currentCustomer.userId);
                break;
            case 6:
                op_status = applyForLoan(sock, currentCustomer.id, currentCustomer.userId);
                break;
            case 7:
                op_status = changeCustomerPassword(sock, currentCustomer.userId);
                break;
            case 8:
                op_status = addFeedback(sock, currentCustomer.userId);
                break;
            case 9: 
                logoutCustomer(session_fd);
                write(sock, "Logged out successfully.\n", 27);
                running = 0;
                break;
            default:
                write(sock, "Invalid choice. Please enter 1-9.\n", 35);
                break;
        }

        if (op_status == 0) {
            logoutCustomer(session_fd); 
            running = 0; 
        }
    }
    return -1; 
}