#include "../include/employee_utils.h"
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h> 

static int getCustomerUsernameByAccountId(int accountId, char *usernameOut);

int validateEmployee(const char *username, const char *password) {
    int fd_user, fd_session;
    User user;
    int foundUserId = -1;

    fd_user = open(USER_FILE, O_RDONLY);
    if (fd_user < 0) {
        perror("open user file");
        return -1;
    }

    while (read(fd_user, &user, sizeof(User)) == sizeof(User)) {
        if (strcmp(user.username, username) == 0 &&
            strcmp(user.password, password) == 0 &&
            (strcmp(user.role, "employee") == 0) &&
            user.isActive == 1) {
            foundUserId = user.id;
            break;
        }
    }
    close(fd_user);

    if (foundUserId == -1) {
        return 0;
    }


    fd_session = open(SESSION_FILE, O_RDWR);
    if (fd_session < 0) {
        perror("open session file");
        return -1;
    }

    Session session;
    off_t session_offset = 0;
    int session_found = 0;
    while (read(fd_session, &session, sizeof(Session)) == sizeof(Session)) {
        if (session.userId == foundUserId) {
            session_found = 1;
            break;
        }
        session_offset += sizeof(Session);
    }

    if (!session_found) {
        close(fd_session);
        write(STDOUT_FILENO, "Error: No session record for user.\n", 37);
        return -1;
    }

    struct flock lock = {0};
    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = session_offset;
    lock.l_len = sizeof(Session);

    if (fcntl(fd_session, F_SETLK, &lock) == -1) {
        close(fd_session);
        return -2;
    }

    return fd_session;
}


void logoutEmployee(int session_fd) {
    if (session_fd < 0)
        return;
    close(session_fd);
}

void addNewCustomer(int sock) {
    User newUser;
    Customer newCustomer;
    int fd_user, fd_cust, fd_session; 
    struct flock user_lock, cust_lock; 
    ssize_t n;
    
    memset(&newUser, 0, sizeof(User));
    memset(&newCustomer, 0, sizeof(Customer));

    write(sock, "Enter Customer Username: ", 26);
    n = readLine(sock, newUser.username, sizeof(newUser.username));
    if (n <= 0) return;

    write(sock, "Enter Password: ", 16);
    n = readLine(sock, newUser.password, sizeof(newUser.password));
    if (n <= 0) return;
    
    char buf[128];
    write(sock, "Enter Initial Deposit Amount: ", 31);
    n = readLine(sock, buf, sizeof(buf));
    if (n <= 0) return;
    
    newCustomer.balance = atof(buf);
    strcpy(newUser.role, "customer");
    newUser.isActive = 1;
    newCustomer.isActive = 1;

    fd_user = open(USER_FILE, O_RDWR | O_CREAT, 0600);
    fd_cust = open(CUSTOMER_FILE, O_RDWR | O_CREAT, 0600);
    fd_session = open(SESSION_FILE, O_WRONLY | O_APPEND | O_CREAT, 0600); 
    
    if (fd_user < 0 || fd_cust < 0 || fd_session < 0) {
        write(sock, "Error: could not open data files.\n", 34);
        if (fd_user >= 0) close(fd_user);
        if (fd_cust >= 0) close(fd_cust);
        if (fd_session >= 0) close(fd_session);
        return;
    }

    memset(&user_lock, 0, sizeof(user_lock));
    user_lock.l_type = F_WRLCK;
    user_lock.l_len = 0; 
    if (fcntl(fd_user, F_SETLKW, &user_lock) == -1) { 
        write(sock, "Error: could not lock user file.\n", 34);
        close(fd_user); close(fd_cust); close(fd_session);
        return;
    }

    memset(&cust_lock, 0, sizeof(cust_lock));
    cust_lock.l_type = F_WRLCK;
    cust_lock.l_len = 0; 
    if (fcntl(fd_cust, F_SETLKW, &cust_lock) == -1) { 
        write(sock, "Error: could not lock customer file.\n", 38);
        user_lock.l_type = F_UNLCK; fcntl(fd_user, F_SETLK, &user_lock);
        close(fd_user); close(fd_cust); close(fd_session);
        return;
    }

    User tempUser;
    int last_user_id = 0;
    lseek(fd_user, 0, SEEK_SET);
    while (read(fd_user, &tempUser, sizeof(User)) == sizeof(User)) {
        if (tempUser.id > last_user_id) last_user_id = tempUser.id;
    }
    newUser.id = last_user_id + 1;
    
    lseek(fd_user, 0, SEEK_END);
    write(fd_user, &newUser, sizeof(User));


    Customer tempCust;
    int last_cust_id = 1000; 
    lseek(fd_cust, 0, SEEK_SET);
    while (read(fd_cust, &tempCust, sizeof(Customer)) == sizeof(Customer)) {
        if (tempCust.id > last_cust_id) last_cust_id = tempCust.id;
    }
    newCustomer.id = last_cust_id + 1;
    newCustomer.userId = newUser.id; 

    lseek(fd_cust, 0, SEEK_END);
    write(fd_cust, &newCustomer, sizeof(Customer));

    Session newSession = {0};
    newSession.userId = newUser.id;
    write(fd_session, &newSession, sizeof(Session));

    close(fd_session); 

    cust_lock.l_type = F_UNLCK;
    fcntl(fd_cust, F_SETLK, &cust_lock);
    close(fd_cust);
    
    user_lock.l_type = F_UNLCK;
    fcntl(fd_user, F_SETLK, &user_lock);
    close(fd_user);


    char msg[256];
    snprintf(msg, sizeof(msg),
             "Customer '%s' added.\nUser ID: %d\nAccount ID: %d\n",
             newUser.username, newUser.id, newCustomer.id);
    write(sock, msg, strlen(msg));
}

void modifyCustomerDetails(int sock) {
    char targetUsername[MAX_NAME];
    write(sock, "Enter customer username to modify: ", 36);
    if (readLine(sock, targetUsername, sizeof(targetUsername)) <= 0)
        return;

    int fd = open(USER_FILE, O_RDWR);
    if (fd < 0) {
        perror("open user file");
        write(sock, "Error opening user file.\n", 25);
        return;
    }

    User user;
    off_t offset = 0;
    int found = 0;


    while (read(fd, &user, sizeof(User)) == sizeof(User)) {
        if (strcmp(user.username, targetUsername) == 0) {
            if (strcmp(user.role, "customer") != 0) {
                write(sock, "This user is not a customer.\n", 31);
                close(fd);
                return;
            }
            found = 1;
            break;
        }
        offset += sizeof(User);
    }

    if (!found) {
        write(sock, "Customer (user) not found.\n", 28);
        close(fd);
        return;
    }

    struct flock lock = {0};
    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = offset;
    lock.l_len = sizeof(User);

    if (fcntl(fd, F_SETLK, &lock) == -1) {
        write(sock, "Record locked by another user. Try again later.\n", 49);
        close(fd);
        return;
    }

    char newName[MAX_NAME], newPassword[MAX_PASS];

    char promptName[128];
    snprintf(promptName, sizeof(promptName), "Current name: %s. Enter new name (or Enter to keep): ", user.username);
    write(sock, promptName, strlen(promptName));
    
    if (readLine(sock, newName, sizeof(newName)) <= 0 || strlen(newName) == 0)
        strcpy(newName, user.username);

    write(sock, "Enter new password (or Enter to keep same): ", 45);
    if (readLine(sock, newPassword, sizeof(newPassword)) <= 0 || strlen(newPassword) == 0)
        strcpy(newPassword, user.password);

    strncpy(user.username, newName, sizeof(user.username) - 1);
    user.username[sizeof(user.username) - 1] = '\0';
    
    strncpy(user.password, newPassword, sizeof(user.password) - 1);
    user.password[sizeof(user.password) - 1] = '\0';

    lseek(fd, offset, SEEK_SET);
    if (write(fd, &user, sizeof(User)) != sizeof(User)) {
        write(sock, "Failed to update record.\n", 26);
    } else {
        write(sock, "Customer login details modified successfully.\n", 47);
        fsync(fd);
    }

    lock.l_type = F_UNLCK;
    fcntl(fd, F_SETLK, &lock);
    close(fd);
}



static int _emp_findAndLockAccount(int accountId, Customer *custOut, off_t *offsetOut) {
    int fd = open(CUSTOMER_FILE, O_RDWR);
    if (fd < 0) return -1;

    off_t offset = 0;
    while(read(fd, custOut, sizeof(Customer)) == sizeof(Customer)) {
        if (custOut->id == accountId) {
            struct flock lock = {0};
            lock.l_type = F_WRLCK;
            lock.l_whence = SEEK_SET;
            lock.l_start = offset;
            lock.l_len = sizeof(Customer);
            if (fcntl(fd, F_SETLKW, &lock) == -1) { 
                close(fd);
                return -2; 
            }
            *offsetOut = offset;
            return fd; 
        }
        offset += sizeof(Customer);
    }
    close(fd);
    return -3;
}

static void _emp_unlockAccount(int fd, off_t offset) {
    struct flock unlock = {0};
    unlock.l_type = F_UNLCK;
    unlock.l_whence = SEEK_SET;
    unlock.l_start = offset;
    unlock.l_len = sizeof(Customer);
    fcntl(fd, F_SETLK, &unlock);
    close(fd);
}

static int _emp_logTransaction(int accountId, const char* type, double amount, int relatedId) {
    int fd_txn = open(TRANSACTION_FILE, O_WRONLY | O_APPEND | O_CREAT, 0600);
    if (fd_txn < 0) return 0;
    Transaction txn;
    txn.timestamp = time(NULL);
    txn.accountId = accountId;
    strncpy(txn.type, type, MAX_TXN_TYPE - 1);
    txn.type[MAX_TXN_TYPE - 1] = '\0';
    txn.amount = amount;
    txn.relatedAccountId = relatedId;

    if (write(fd_txn, &txn, sizeof(Transaction)) != sizeof(Transaction)) {
        close(fd_txn);
        return 0; 
    }
    fsync(fd_txn);
    close(fd_txn);
    return 1; 
}

void processLoan(int sock, int employeeId) {
    char buffer[128], choiceBuf[16];
    int loanId, choice;
    Loan loan;
    Customer cust;
    off_t loanOffset, custOffset;
    int fd_loan = -1, fd_cust = -1;
    char custUsername[MAX_NAME];


    write(sock, "Enter Loan ID to process: ", 27);
    if (readLine(sock, buffer, sizeof(buffer)) <= 0) return;
    loanId = atoi(buffer);
    if (loanId <= 0) {
        write(sock, "Invalid Loan ID.\n", 17);
        return;
    }

    fd_loan = open(LOAN_FILE, O_RDWR);
    if (fd_loan < 0) {
        write(sock, "Error opening loan file.\n", 26);
        return;
    }
    
    loanOffset = 0;
    int found = 0;
    while(read(fd_loan, &loan, sizeof(Loan)) == sizeof(Loan)) {
        if (loan.loanId == loanId) {
            found = 1;
            break;
        }
        loanOffset += sizeof(Loan);
    }
    
    if (!found) {
        write(sock, "Loan ID not found.\n", 20);
        close(fd_loan);
        return;
    }

    struct flock loanLock = {0};
    loanLock.l_type = F_WRLCK;
    loanLock.l_whence = SEEK_SET;
    loanLock.l_start = loanOffset;
    loanLock.l_len = sizeof(Loan);
    if (fcntl(fd_loan, F_SETLKW, &loanLock) == -1) {
        write(sock, "Loan record is busy. Try again later.\n", 40);
        close(fd_loan);
        return;
    }

    if (loan.employeeId != employeeId) {
        write(sock, "This loan is not assigned to you.\n", 36);
        goto cleanup_loan_lock; 
    }
    if (strcmp(loan.status, "PENDING") != 0) {
        snprintf(buffer, sizeof(buffer), "This loan has already been %s.\n", loan.status);
        write(sock, buffer, strlen(buffer));
        goto cleanup_loan_lock;
    }

    if (getCustomerUsernameByAccountId(loan.accountId, custUsername)) {
        snprintf(buffer, sizeof(buffer), "\n-- Processing Loan ID %d --\nCustomer: %s (Acc: %d)\nAmount: $%.2f\nStatus: %s\n",
                 loan.loanId, custUsername, loan.accountId, loan.amount, loan.status);
    } else {
        snprintf(buffer, sizeof(buffer), "\n-- Processing Loan ID %d --\nCustomer: [Unknown] (Acc: %d)\nAmount: $%.2f\nStatus: %s\n",
                 loan.loanId, loan.accountId, loan.amount, loan.status);
    }
    write(sock, buffer, strlen(buffer));
    
    write(sock, "\nAction:\n1. Approve\n2. Reject\n3. Cancel\nEnter choice: ", 56);
    if (readLine(sock, choiceBuf, sizeof(choiceBuf)) <= 0) {
        goto cleanup_loan_lock;
    }
    choice = atoi(choiceBuf);

    switch(choice) {
        case 1: 
        {

            fd_cust = _emp_findAndLockAccount(loan.accountId, &cust, &custOffset);
            if (fd_cust == -1) {
                write(sock, "Error: Could not open customer file. Aborting.\n", 49);
                goto cleanup_loan_lock;
            }
            if (fd_cust == -2) {
                write(sock, "Error: Customer account is busy. Aborting.\n", 45);
                goto cleanup_loan_lock;
            }
            if (fd_cust == -3) {
                write(sock, "Error: Could not find customer account. Aborting.\n", 51);
                goto cleanup_loan_lock;
            }


            double oldBalance = cust.balance;
            char oldStatus[MAX_STATUS];
            strcpy(oldStatus, loan.status);


            cust.balance += loan.amount;
            strcpy(loan.status, "APPROVED");

            int write1_ok = 0, write2_ok = 0, log_ok = 0;

            lseek(fd_cust, custOffset, SEEK_SET);
            if (write(fd_cust, &cust, sizeof(Customer)) == sizeof(Customer)) {
                write1_ok = 1;
            }

            lseek(fd_loan, loanOffset, SEEK_SET);
            if (write(fd_loan, &loan, sizeof(Loan)) == sizeof(Loan)) {
                write2_ok = 1;
            }

            if (write1_ok && write2_ok) {
                if (_emp_logTransaction(loan.accountId, "LOAN", loan.amount, -1)) {
                    log_ok = 1;
                }
            }

            if (write1_ok && write2_ok && log_ok) {

                fsync(fd_cust);
                fsync(fd_loan);
                write(sock, "Loan APPROVED. Funds transferred.\n", 35);
            } else {

                write(sock, "CRITICAL: Transaction failed. Rolling back.\n", 46);
                
                cust.balance = oldBalance;
                lseek(fd_cust, custOffset, SEEK_SET);
                write(fd_cust, &cust, sizeof(Customer));
                fsync(fd_cust);

                strcpy(loan.status, oldStatus);
                lseek(fd_loan, loanOffset, SEEK_SET);
                write(fd_loan, &loan, sizeof(Loan));
                fsync(fd_loan);
            }
            
            _emp_unlockAccount(fd_cust, custOffset);
            break;
        }
        case 2: 
        {
            strcpy(loan.status, "REJECTED");
            lseek(fd_loan, loanOffset, SEEK_SET);
            if (write(fd_loan, &loan, sizeof(Loan)) != sizeof(Loan)) {
                write(sock, "Error updating loan status.\n", 30);
            } else {
                fsync(fd_loan);
                write(sock, "Loan REJECTED successfully.\n", 30);
            }
            break;
        }
        case 3:
            write(sock, "Operation cancelled.\n", 21);
            break;
        default:
            write(sock, "Invalid choice.\n", 17);
            break;
    }

cleanup_loan_lock:
    loanLock.l_type = F_UNLCK;
    fcntl(fd_loan, F_SETLK, &loanLock);
    close(fd_loan);
}


static int getCustomerUsernameByAccountId(int accountId, char *usernameOut) {
    int fd_cust, fd_user;
    Customer cust;
    User user;
    int targetUserId = -1;


    fd_cust = open(CUSTOMER_FILE, O_RDONLY);
    if (fd_cust < 0) return 0; 

    while(read(fd_cust, &cust, sizeof(Customer)) == sizeof(Customer)) {
        if (cust.id == accountId) {
            targetUserId = cust.userId;
            break;
        }
    }
    close(fd_cust);
    if (targetUserId == -1) return 0; 


    fd_user = open(USER_FILE, O_RDONLY);
    if (fd_user < 0) return 0; 

    while(read(fd_user, &user, sizeof(User)) == sizeof(User)) {
        if (user.id == targetUserId) {
            strcpy(usernameOut, user.username);
            close(fd_user);
            return 1; 
        }
    }
    close(fd_user);
    return 0; 
}


void viewAssignedLoans(int sock, int employeeId) {
    int fd_loan = open(LOAN_FILE, O_RDONLY);
    if (fd_loan < 0) {
        write(sock, "No loans found in the system.\n", 32);
        return;
    }

    struct flock lock = {0};
    lock.l_type = F_RDLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;

    if (fcntl(fd_loan, F_SETLK, &lock) == -1) {
        write(sock, "Loan file is busy. Try again later.\n", 38);
        close(fd_loan);
        return;
    }

    write(sock, "\n--- Your Assigned PENDING Loans ---\n", 39);
    Loan loan;
    char buffer[256];
    char custUsername[MAX_NAME];
    int count = 0;

    while(read(fd_loan, &loan, sizeof(Loan)) == sizeof(Loan)) {

        if (loan.employeeId == employeeId && strcmp(loan.status, "PENDING") == 0) {

            if (getCustomerUsernameByAccountId(loan.accountId, custUsername)) {
                snprintf(buffer, sizeof(buffer),
                         "  - Loan ID: %d | Customer: %s (Acc: %d) | Amount: $%.2f\n",
                         loan.loanId, custUsername, loan.accountId, loan.amount);
            } else {
                snprintf(buffer, sizeof(buffer),
                         "  - Loan ID: %d | Customer: [Unknown] (Acc: %d) | Amount: $%.2f\n",
                         loan.loanId, loan.accountId, loan.amount);
            }
            write(sock, buffer, strlen(buffer));
            count++;
        }
    }
    
    if (count == 0) {
        write(sock, "  (No pending loans assigned to you)\n", 38);
    }
    write(sock, "-------------------------------------\n", 38);


    lock.l_type = F_UNLCK;
    fcntl(fd_loan, F_SETLK, &lock);
    close(fd_loan);
}

void viewCustomerTransactions(int sock) {
    char targetUsername[MAX_NAME];
    char buffer[512];
    int targetUserId = -1;
    int targetAccountId = -1;
    User user;
    Customer cust;

    write(sock, "Enter customer username to view transactions: ", 47);
    if (readLine(sock, targetUsername, sizeof(targetUsername)) <= 0) {
        return; 
    }

    int fd_user = open(USER_FILE, O_RDONLY);
    if (fd_user < 0) {
        write(sock, "Error opening user file.\n", 25);
        return;
    }
    while(read(fd_user, &user, sizeof(User)) == sizeof(User)) {
        if (strcmp(user.username, targetUsername) == 0 &&
            strcmp(user.role, "customer") == 0) {
            targetUserId = user.id;
            break;
        }
    }
    close(fd_user);
    if (targetUserId == -1) {
        write(sock, "Customer (user) not found.\n", 28);
        return;
    }

    int fd_cust = open(CUSTOMER_FILE, O_RDONLY);
    if (fd_cust < 0) {
        write(sock, "Error opening customer file.\n", 30);
        return;
    }
    while(read(fd_cust, &cust, sizeof(Customer)) == sizeof(Customer)) {
        if (cust.userId == targetUserId) {
            targetAccountId = cust.id;
            break;
        }
    }
    close(fd_cust);
    if (targetAccountId == -1) {
        write(sock, "Customer (account) not found.\n", 31);
        return;
    }

    int fd_txn = open(TRANSACTION_FILE, O_RDONLY);
    if (fd_txn < 0) {
        write(sock, "No transaction history found for this user.\n", 46);
        return;
    }

    struct flock lock = {0};
    lock.l_type = F_RDLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;
    
    if (fcntl(fd_txn, F_SETLK, &lock) == -1) {
        write(sock, "Transaction log is busy. Try again later.\n", 43);
        close(fd_txn);
        return;
    }

    Transaction txn;
    int count = 0;
    snprintf(buffer, sizeof(buffer), "\n--- Transaction History for %s (Account %d) ---\n",
             targetUsername, targetAccountId);
    write(sock, buffer, strlen(buffer));

    while (read(fd_txn, &txn, sizeof(Transaction)) == sizeof(Transaction)) {
        if (txn.accountId == targetAccountId || txn.relatedAccountId == targetAccountId) {
            char timeStr[30];
            strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M", localtime(&txn.timestamp));

            if (txn.accountId == targetAccountId) {
                if (strcmp(txn.type, "TRANSFER") == 0) {
                    snprintf(buffer, sizeof(buffer), "[%s] %s: Sent $%.2f to Account %d\n",
                             timeStr, txn.type, txn.amount, txn.relatedAccountId);
                } else {
                    snprintf(buffer, sizeof(buffer), "[%s] %s: $%.2f\n",
                             timeStr, txn.type, txn.amount);
                }
            } else {
                snprintf(buffer, sizeof(buffer), "[%s] TRANSFER: Received $%.2f from Account %d\n",
                         timeStr, txn.amount, txn.accountId);
            }

            write(sock, buffer, strlen(buffer));
            count++;
        }
    }

    if (count == 0) {
        write(sock, "No transactions found.\n", 24);
    }

    lock.l_type = F_UNLCK;
    fcntl(fd_txn, F_SETLK, &lock);
    close(fd_txn);
}
void changeEmployeePassword(int sock, int userId) {
    char newPass[MAX_PASS], confirmPass[MAX_PASS];
    User userRecord;

    write(sock, "Enter new password: ", 21);
    if (readLine(sock, newPass, sizeof(newPass)) <= 0) return;

    write(sock, "Confirm new password: ", 23);
    if (readLine(sock, confirmPass, sizeof(confirmPass)) <= 0) return;

    if (strcmp(newPass, confirmPass) != 0) {
        write(sock, "Passwords do not match.\n", 25);
        return;
    }

    int fd = open(USER_FILE, O_RDWR);
    if (fd < 0) {
        write(sock, "Error opening user file.\n", 25);
        return;
    }

    off_t offset = 0;
    int found = 0;
    while (read(fd, &userRecord, sizeof(User)) == sizeof(User)) {
        if (userRecord.id == userId) {
            found = 1;
            break;
        }
        offset += sizeof(User);
    }

    if (!found) {
        write(sock, "Error: Could not find user record.\n", 36);
        close(fd);
        return;
    }

    struct flock lock = {0};
    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = offset;
    lock.l_len = sizeof(User);

    if (fcntl(fd, F_SETLK, &lock) == -1) {
        write(sock, "Record locked by another user. Try again later.\n", 49);
        close(fd);
        return;
    }

    strncpy(userRecord.password, newPass, sizeof(userRecord.password) - 1);
    userRecord.password[sizeof(userRecord.password) - 1] = '\0';

    lseek(fd, offset, SEEK_SET);
    if (write(fd, &userRecord, sizeof(User)) != sizeof(User)) {
        write(sock, "Failed to update password.\n", 28);
    } else {
        fsync(fd);
        write(sock, "Password updated successfully.\n", 33);
    }

    lock.l_type = F_UNLCK;
    fcntl(fd, F_SETLK, &lock);
    close(fd);
}