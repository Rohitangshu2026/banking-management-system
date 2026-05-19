#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>

#include "../include/common.h"
#include "../include/customer.h"
#include "../include/customer_utils.h"

int validateCustomer(const char *username, const char *password, Customer *customerOut) {
    int fd_user, fd_session, fd_cust;
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
            strcmp(user.role, "customer") == 0 &&
            user.isActive == 1) 
        {
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

    fd_cust = open(CUSTOMER_FILE, O_RDONLY);
    if (fd_cust < 0) {
        perror("open customer file");
        close(fd_session);
        return -1;
    }

    Customer cust;
    int account_found = 0;
    while (read(fd_cust, &cust, sizeof(Customer)) == sizeof(Customer)) {
        if (cust.userId == foundUserId && cust.isActive == 1) {
            *customerOut = cust;
            account_found = 1;
            break;
        }
    }
    close(fd_cust);

    if (!account_found) {
        write(STDOUT_FILENO, "Error: No active customer account for user.\n", 45);
        close(fd_session);
        return 0;
    }

    /* TOCTOU close: between the User.isActive check above (before the
     * session lock) and reaching this point, an admin or manager could
     * have flipped the active flag. Re-read the User record while
     * holding the session lock and bail out if so. */
    int fd_user_recheck = open(USER_FILE, O_RDONLY);
    if (fd_user_recheck >= 0) {
        User u;
        int still_active = 0;
        while (read(fd_user_recheck, &u, sizeof(User)) == sizeof(User)) {
            if (u.id == foundUserId) {
                still_active = (u.isActive == 1);
                break;
            }
        }
        close(fd_user_recheck);
        if (!still_active) {
            close(fd_session);
            return 0;
        }
    }

    return fd_session;
}

void logoutCustomer(int session_fd) {
    if (session_fd < 0)
        return;
    close(session_fd);
}

static int getAccountByUsername(int sock, const char *username, Customer *custOut, off_t *offsetOut) {
    int fd_user, fd_cust;
    User user;
    int foundUserId = -1;

    fd_user = open(USER_FILE, O_RDONLY);
    if (fd_user < 0) {
        perror("getAccountByUsername: open USER_FILE");
        write(sock, "Error checking user file.\n", 27);
        return -1;
    }

    while (read(fd_user, &user, sizeof(User)) == sizeof(User)) {
        if (strcmp(user.username, username) == 0 && 
            strcmp(user.role, "customer") == 0) {

            if (user.isActive == 0) {
                write(sock, "Target customer's user account is deactivated.\n", 48);
                close(fd_user);
                return -1;
            }

            
            foundUserId = user.id;
            break;
        }
    }
    close(fd_user);

    if (foundUserId == -1) {
        write(sock, "Target user not found or is not a customer.\n", 46);
        return -1;
    }

    fd_cust = open(CUSTOMER_FILE, O_RDONLY);
    if (fd_cust < 0) {
        perror("getAccountByUsername: open CUSTOMER_FILE");
        write(sock, "Error checking customer file.\n", 32);
        return -1;
    }

    off_t offset = 0;
    while(read(fd_cust, custOut, sizeof(Customer)) == sizeof(Customer)) {
        if (custOut->userId == foundUserId) {
            if (custOut->isActive == 0) {
                write(sock, "Target customer's account is closed.\n", 39);
                close(fd_cust);
                return -1;
            }
            *offsetOut = offset;
            close(fd_cust);
            return custOut->id;
        }
        offset += sizeof(Customer);
    }

    write(sock, "Target customer account not found.\a\n", 38);
    close(fd_cust);
    return -1;
}

int viewBalance(int sock, int accountId, int userId) {
    
    if (!isUserActive(userId)) {
        write(sock, "Your account has been deactivated by a manager. Logging out.\n", 64);
        return 0;
    }

    int fd = open(CUSTOMER_FILE, O_RDONLY);
    if (fd < 0) {
        write(sock, "Error reading account details.\n", 32);
        return 1;
    }

    struct flock lock = {0};
    lock.l_type = F_RDLCK; 
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0; 
    
    if (fcntl(fd, F_SETLKW, &lock) == -1) { 
        perror("viewBalance: fcntl read lock");
        write(sock, "Error locking account file. Try again.\n", 40);
        close(fd);
        return 1;
    }

    Customer cust;
    int found = 0;
    lseek(fd, 0, SEEK_SET); 
    
    while(read(fd, &cust, sizeof(Customer)) == sizeof(Customer)) {
        if (cust.id == accountId) {
            found = 1;
            break;
        }
    }

    lock.l_type = F_UNLCK;
    fcntl(fd, F_SETLK, &lock);
    close(fd);

    if (!found) {
        write(sock, "Error: Account not found.\n", 27);
        return 1;
    }

    char buffer[128];
    snprintf(buffer, sizeof(buffer), "Your current balance is: $%.2f\n", cust.balance);
    write(sock, buffer, strlen(buffer));

    return 1;
}

static int findAndLockAccount(int sock, int accountId, Customer *custOut, off_t *offsetOut) {
    int fd = open(CUSTOMER_FILE, O_RDWR); 
    if (fd < 0) {
        write(sock, "Error opening account file.\n", 30);
        return -1;
    }

    off_t offset = 0;
    while(read(fd, custOut, sizeof(Customer)) == sizeof(Customer)) {
        if (custOut->id == accountId) {
            struct flock lock = {0};
            lock.l_type = F_WRLCK;
            lock.l_whence = SEEK_SET;
            lock.l_start = offset;
            lock.l_len = sizeof(Customer);

            if (fcntl(fd, F_SETLK, &lock) == -1) { 
                write(sock, "Account busy. Try again later.\n", 33);
                close(fd);
                return -1;
            }
            *offsetOut = offset;
            return fd; 
        }
        offset += sizeof(Customer);
    }

    write(sock, "Error: Account not found.\n", 27);
    close(fd);
    return -1;
}

static void unlockAccount(int fd, off_t offset) {
    struct flock unlock = {0};
    unlock.l_type = F_UNLCK;
    unlock.l_whence = SEEK_SET;
    unlock.l_start = offset;
    unlock.l_len = sizeof(Customer);
    fcntl(fd, F_SETLK, &unlock);
    close(fd);
}

static int logTransaction(int accountId, const char* type, double amount, int relatedId) {
    int fd_txn = open(TRANSACTION_FILE, O_WRONLY | O_APPEND | O_CREAT, 0600);
    if (fd_txn < 0) {
        perror("logTransaction: open TRANSACTION_FILE");
        return 0;
    }

    Transaction txn;
    txn.timestamp = time(NULL);
    txn.accountId = accountId;
    strncpy(txn.type, type, MAX_TXN_TYPE - 1);
    txn.type[MAX_TXN_TYPE - 1] = '\0';
    txn.amount = amount;
    txn.relatedAccountId = relatedId;

    if (write(fd_txn, &txn, sizeof(Transaction)) != sizeof(Transaction)) {
        perror("logTransaction: write");
        close(fd_txn);
        return 0; 
    }

    fsync(fd_txn);
    close(fd_txn);
    return 1; 
}

int depositMoney(int sock, int accountId, int userId) {
    
    if (!isUserActive(userId)) {
        write(sock, "Your account has been deactivated by a manager. Logging out.\n", 64);
        return 0; 
    }
    
    Customer cust;
    off_t offset;
    char buffer[128];
    double amount;

    write(sock, "Enter amount to deposit: ", 26);
    if (readLine(sock, buffer, sizeof(buffer)) <= 0) return 1;
    amount = atof(buffer);
    if (amount <= 0) {
        write(sock, "Invalid deposit amount.\n", 25);
        return 1;
    }

    if (!isUserActive(userId)) {
        write(sock, "Your account was deactivated while entering. Transaction cancelled.\n", 70);
        return 0;
    }

    int fd = findAndLockAccount(sock, accountId, &cust, &offset);
    if (fd < 0) return 1;

    double oldBalance = cust.balance;
    cust.balance += amount;

    lseek(fd, offset, SEEK_SET);
    if (write(fd, &cust, sizeof(Customer)) != sizeof(Customer)) {
        write(sock, "Error updating balance. Operation cancelled.\n", 46);
        unlockAccount(fd, offset);
        return 1;
    }
    fsync(fd);

    if (!logTransaction(accountId, "DEPOSIT", amount, -1)) {
        write(sock, "CRITICAL: Transaction log failed. Rolling back balance change.\n", 66);
        cust.balance = oldBalance;
        lseek(fd, offset, SEEK_SET);
        if (write(fd, &cust, sizeof(Customer)) != sizeof(Customer)) {
             write(sock, "CRITICAL: ROLLBACK FAILED. Contact admin.\n", 45);
        } else {
             fsync(fd);
        }
        unlockAccount(fd, offset);
        return 1;
    }
    
    snprintf(buffer, sizeof(buffer), "Deposited $%.2f. New balance: $%.2f\n", amount, cust.balance);
    write(sock, buffer, strlen(buffer));

    unlockAccount(fd, offset);
    return 1; 
}

int withdrawMoney(int sock, int accountId, int userId) {
    
    if (!isUserActive(userId)) {
        write(sock, "Your account has been deactivated by a manager. Logging out.\n", 63);
        return 0;
    }

    Customer cust;
    off_t offset;
    char buffer[128];
    double amount;

    write(sock, "Enter amount to withdraw: ", 27);
    if (readLine(sock, buffer, sizeof(buffer)) <= 0) return 1;
    amount = atof(buffer);
    if (amount <= 0) {
        write(sock, "Invalid withdrawal amount.\n", 27);
        return 1;
    }

    if (!isUserActive(userId)) {
        write(sock, "Your account was deactivated while entering. Transaction cancelled.\n", 68);
        return 0;
    }

    int fd = findAndLockAccount(sock, accountId, &cust, &offset);
    if (fd < 0) return 1;

    if (amount > cust.balance) {
        snprintf(buffer, sizeof(buffer), "Insufficient funds. You only have $%.2f.\n", cust.balance);
        write(sock, buffer, strlen(buffer));
        unlockAccount(fd, offset);
        return 1;
    }

    double oldBalance = cust.balance;
    cust.balance -= amount;

    lseek(fd, offset, SEEK_SET);
    if (write(fd, &cust, sizeof(Customer)) != sizeof(Customer)) {
        write(sock, "Error updating balance. Operation cancelled.\n", 45);
        unlockAccount(fd, offset);
        return 1;
    }
    fsync(fd);

    if (!logTransaction(accountId, "WITHDRAW", amount, -1)) {
        write(sock, "CRITICAL: Transaction log failed. Rolling back balance change.\n", 63);
        cust.balance = oldBalance;
        lseek(fd, offset, SEEK_SET);
        if (write(fd, &cust, sizeof(Customer)) != sizeof(Customer)) {
             write(sock, "CRITICAL: ROLLBACK FAILED. Contact admin.\n", 42);
        } else {
             fsync(fd);
        }
        unlockAccount(fd, offset);
        return 1;
    }
    
    snprintf(buffer, sizeof(buffer), "Withdrew $%.2f. New balance: $%.2f\n", amount, cust.balance);
    write(sock, buffer, strlen(buffer));

    unlockAccount(fd, offset);
    return 1;
}

int changeCustomerPassword(int sock, int userId) {
    
    if (!isUserActive(userId)) {
        write(sock, "Your account has been deactivated by a manager. Logging out.\n", 61);
        return 0;
    }

    char newPass[MAX_PASS], confirmPass[MAX_PASS];

    write(sock, "Enter new password: ", 20);
    if (readLine(sock, newPass, sizeof(newPass)) <= 0) return 1; 

    write(sock, "Confirm new password: ", 21);
    if (readLine(sock, confirmPass, sizeof(confirmPass)) <= 0) return 1;

    if (!isUserActive(userId)) {
        write(sock, "Your account was deactivated while entering. Password change cancelled.\n", 72);
        return 0;
    }

    if (strcmp(newPass, confirmPass) != 0) {
        write(sock, "Passwords do not match.\n", 24);
        return 1;
    }

    int fd_user = open(USER_FILE, O_RDWR);
    if (fd_user < 0) {
        write(sock, "Error opening user file.\n", 25);
        return 1;
    }

    User user;
    off_t offset = 0;
    int found = 0;
    while (read(fd_user, &user, sizeof(User)) == sizeof(User)) {
        if (user.id == userId) {
            found = 1;
            break;
        }
        offset += sizeof(User);
    }

    if (!found) {
        write(sock, "Error: User record not found.\n", 30);
        close(fd_user);
        return 1;
    }

    struct flock lock = {0};
    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = offset;
    lock.l_len = sizeof(User);

    if (fcntl(fd_user, F_SETLK, &lock) == -1) {
        write(sock, "Record locked by admin. Try later.\n", 35);
        close(fd_user);
        return 1;
    }

    strncpy(user.password, newPass, sizeof(user.password) - 1);
    user.password[sizeof(user.password) - 1] = '\0';

    lseek(fd_user, offset, SEEK_SET);
    if (write(fd_user, &user, sizeof(User)) != sizeof(User)) {
        write(sock, "Failed to update password.\n", 27);
    } else {
        write(sock, "Password updated successfully.\n", 31);
        fsync(fd_user);
    }

    lock.l_type = F_UNLCK;
    fcntl(fd_user, F_SETLK, &lock);
    close(fd_user);

    return 1; 
}

int viewTransactionHistory(int sock, int accountId, int userId) {
    if (!isUserActive(userId)) {
        write(sock, "Your account has been deactivated by a manager. Logging out.\n", 61);
        return 0;
    }

    int fd_txn = open(TRANSACTION_FILE, O_RDONLY);
    if (fd_txn < 0) {
        write(sock, "No transaction history found.\n", 30);
        return 1;
    }

    struct flock lock = {0};
    lock.l_type = F_RDLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0; 
    
    if (fcntl(fd_txn, F_SETLK, &lock) == -1) {
        write(sock, "Transaction log is busy. Try again later.\n", 42);
        close(fd_txn);
        return 1;
    }

    Transaction txn;
    char buffer[512];
    int count = 0;

    write(sock, "\n--- Your Transaction History ---\n", 33);

    while (read(fd_txn, &txn, sizeof(Transaction)) == sizeof(Transaction)) {
        if (txn.accountId == accountId || txn.relatedAccountId == accountId) {
            char timeStr[30];
            strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M", localtime(&txn.timestamp));

            if (txn.accountId == accountId) {
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
        write(sock, "No transactions found.\n", 23);
    }

    lock.l_type = F_UNLCK;
    fcntl(fd_txn, F_SETLK, &lock);
    close(fd_txn);

    return 1;
}


int transferFunds(int sock, int sourceAccountId, int userId) {
    
    if (!isUserActive(userId)) {
        write(sock, "Your account has been deactivated by a manager. Logging out.\n", 61);
        return 0;
    }

    Customer sourceCust, targetCust;
    off_t targetOffset; 
    int targetAccountId;
    double amount;
    char buffer[128];
    char targetUsername[MAX_NAME];

    write(sock, "Enter target username to transfer to: ", 38);
    if (readLine(sock, targetUsername, sizeof(targetUsername)) <= 0) return 1;

    write(sock, "Enter amount to transfer: ", 25);
    if (readLine(sock, buffer, sizeof(buffer)) <= 0) return 1;
    amount = atof(buffer);

    if (amount <= 0) {
        write(sock, "Invalid transfer amount.\n", 25);
        return 1;
    }

    if (!isUserActive(userId)) {
        write(sock, "Your account was deactivated while entering. Transaction cancelled.\n", 68);
        return 0;
    }

    targetAccountId = getAccountByUsername(sock, targetUsername, &targetCust, &targetOffset);
    
    if (targetAccountId == -1) {
        return 1;
    }

    if (targetAccountId == sourceAccountId) {
        write(sock, "You cannot transfer money to your own account.\n", 47);
        return 1;
    }
    
    int fd1, fd2;
    off_t offset1, offset2;
    Customer cust1, cust2;

    int firstLockId = (sourceAccountId < targetAccountId) ? sourceAccountId : targetAccountId;
    int secondLockId = (sourceAccountId < targetAccountId) ? targetAccountId : sourceAccountId;

    fd1 = findAndLockAccount(sock, firstLockId, &cust1, &offset1);
    if (fd1 < 0) {
        return 1;
    }

    fd2 = findAndLockAccount(sock, secondLockId, &cust2, &offset2);
    if (fd2 < 0) {
        unlockAccount(fd1, offset1);
        return 1;
    }

    if (cust1.id == sourceAccountId) {
        sourceCust = cust1;
        targetCust = cust2;
    } else {
        sourceCust = cust2;
        targetCust = cust1;
    }
    
    if (sourceCust.balance < amount) {
        write(sock, "Insufficient funds for transfer.\n", 33);
        unlockAccount(fd1, offset1);
        unlockAccount(fd2, offset2);
        return 1;
    }

    double oldSourceBalance = sourceCust.balance;
    double oldTargetBalance = targetCust.balance;
    sourceCust.balance -= amount;
    targetCust.balance += amount;

    int write1_ok = 0, write2_ok = 0;

    lseek(fd1, offset1, SEEK_SET);
    if (write(fd1, (cust1.id == sourceAccountId ? &sourceCust : &targetCust), sizeof(Customer)) == sizeof(Customer)) {
        fsync(fd1);
        write1_ok = 1;
    }

    lseek(fd2, offset2, SEEK_SET);
    if (write(fd2, (cust2.id == targetAccountId ? &targetCust : &sourceCust), sizeof(Customer)) == sizeof(Customer)) {
        fsync(fd2);
        write2_ok = 1;
    }
    
    if (!write1_ok || !write2_ok) {
        write(sock, "CRITICAL: File write error. Rolling back transaction.\n", 54);
        sourceCust.balance = oldSourceBalance;
        targetCust.balance = oldTargetBalance;
        
        lseek(fd1, offset1, SEEK_SET);
        write(fd1, (cust1.id == sourceAccountId ? &sourceCust : &targetCust), sizeof(Customer));
        fsync(fd1);
        
        lseek(fd2, offset2, SEEK_SET);
        write(fd2, (cust2.id == targetAccountId ? &targetCust : &sourceCust), sizeof(Customer));
        fsync(fd2);
        
        unlockAccount(fd1, offset1);
        unlockAccount(fd2, offset2);
        return 1;
    }

    if (!logTransaction(sourceAccountId, "TRANSFER", amount, targetAccountId)) {
        write(sock, "CRITICAL: Transaction log failed. Rolling back transaction.\n", 60);
        sourceCust.balance = oldSourceBalance;
        targetCust.balance = oldTargetBalance;
        
        lseek(fd1, offset1, SEEK_SET);
        write(fd1, (cust1.id == sourceAccountId ? &sourceCust : &targetCust), sizeof(Customer));
        fsync(fd1);
        
        lseek(fd2, offset2, SEEK_SET);
        write(fd2, (cust2.id == targetAccountId ? &targetCust : &sourceCust), sizeof(Customer));
        fsync(fd2);
        
        unlockAccount(fd1, offset1);
        unlockAccount(fd2, offset2);
        return 1;
    }

    snprintf(buffer, sizeof(buffer), "Transferred $%.2f to %s (Account %d).\nYour new balance is $%.2f\n",
             amount, targetUsername, targetAccountId, sourceCust.balance);
    write(sock, buffer, strlen(buffer));

    unlockAccount(fd1, offset1);
    unlockAccount(fd2, offset2);

    return 1;
}

int applyForLoan(int sock, int sourceAccountId, int userId) {
    if (!isUserActive(userId)) {
        write(sock, "Your account has been deactivated by a manager. Logging out.\n", 61);
        return 0; 
    }

    char buffer[128];
    double amount;
 
    write(sock, "Enter amount you wish to apply for: $", 36);
    if (readLine(sock, buffer, sizeof(buffer)) <= 0) return 1;
    amount = atof(buffer);

    if (amount <= 0) {
        write(sock, "Invalid loan amount.\n", 21);
        return 1;
    }

    if (!isUserActive(userId)) {
        write(sock, "Your account was deactivated while entering. Transaction cancelled.\n", 68);
        return 0; 
    }

    int fd_loan = open(LOAN_FILE, O_RDWR | O_CREAT, 0600);
    if (fd_loan < 0) {
        perror("applyForLoan: open LOAN_FILE");
        write(sock, "Error accessing loan system. Please try again.\n", 47);
        return 1;
    }

    struct flock lock = {0};
    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0; 
    
    if (fcntl(fd_loan, F_SETLKW, &lock) == -1) {
        perror("applyForLoan: fcntl F_SETLKW");
        write(sock, "Loan system is busy. Please try again.\n", 39);
        close(fd_loan);
        return 1;
    }

    Loan tempLoan;
    int last_id = LOAN_ID_BASE;
    lseek(fd_loan, 0, SEEK_SET);
    while (read(fd_loan, &tempLoan, sizeof(Loan)) == sizeof(Loan)) {
        if (tempLoan.loanId > last_id) {
            last_id = tempLoan.loanId;
        }
    }
    if (last_id >= MAX_AUTO_ID) {
        write(sock, "Loan registry is full. Contact administrator.\n", 47);
        struct flock unlock = {0};
        unlock.l_type = F_UNLCK;
        unlock.l_whence = SEEK_SET;
        fcntl(fd_loan, F_SETLK, &unlock);
        close(fd_loan);
        return 1;
    }

    Loan newLoan;
    newLoan.loanId = last_id + 1;
    newLoan.accountId = sourceAccountId;
    newLoan.amount = amount;
    newLoan.timestamp = time(NULL);
    newLoan.employeeId = -1;
    safe_strcpy(newLoan.status, "PENDING", sizeof(newLoan.status));

    lseek(fd_loan, 0, SEEK_END);
    if (write(fd_loan, &newLoan, sizeof(Loan)) != sizeof(Loan)) {
        write(sock, "Failed to save loan application.\n", 33);
    } else {
        fsync(fd_loan);
        snprintf(buffer, sizeof(buffer), "Loan application submitted. Your Loan ID is %d\n", newLoan.loanId);
        write(sock, buffer, strlen(buffer));
    }

    lock.l_type = F_UNLCK;
    fcntl(fd_loan, F_SETLK, &lock);
    close(fd_loan);

    return 1; 
}
int addFeedback(int sock, int userId) {
    if (!isUserActive(userId)) {
        write(sock, "Your account has been deactivated. Logging out.\n", 47);
        return 0;
    }

    char buffer[MAX_FEEDBACK];
    write(sock, "Enter your feedback (max 512 chars):\n> ", 39);
    if (readLine(sock, buffer, sizeof(buffer)) <= 0) {
        return 1; 
    }

    if (!isUserActive(userId)) {
        write(sock, "Your account was deactivated. Feedback not sent.\n", 48);
        return 0; 
    }

    int fd_fb = open(FEEDBACK_FILE, O_WRONLY | O_APPEND | O_CREAT, 0600);
    if (fd_fb < 0) {
        perror("addFeedback: open FEEDBACK_FILE");
        write(sock, "Error saving feedback. Please try again.\n", 41);
        return 1;
    }

    Feedback fb;
    fb.timestamp = time(NULL);
    fb.userId = userId;
    strncpy(fb.message, buffer, MAX_FEEDBACK - 1);
    fb.message[MAX_FEEDBACK - 1] = '\0';

    if (write(fd_fb, &fb, sizeof(Feedback)) != sizeof(Feedback)) {
        perror("addFeedback: write");
        write(sock, "Error saving feedback. Please try again.\n", 41);
    } else {
        fsync(fd_fb);
        write(sock, "Thank you! Your feedback has been submitted.\n", 45);
    }

    close(fd_fb);
    return 1;
}