#ifndef COMMON_H
#define COMMON_H

#include <stddef.h>  
#include <sys/types.h> 
#include <time.h>     



#define USER_FILE     "data/users.txt"
#define LOG_FILE      "data/logs.txt"
#define ADMIN_FILE    "data/admins.txt"
#define CUSTOMER_FILE "data/customers.txt"
#define SESSION_FILE  "data/sessions.txt"
#define TRANSACTION_FILE "data/transactions.txt"
#define FEEDBACK_FILE "data/feedback.txt"
#define LOAN_FILE     "data/loans.txt" 

ssize_t readLine(int sock, char *buf, size_t size);
int isUserActive(int userId);
int checkUserRole(int userId, const char* expectedRole);

/*
 * parse_int_strict: parse `s` as a decimal integer into `*out`.
 * Returns 1 on success, 0 if the string is empty, contains non-digit
 * characters (after optional sign), or overflows int. atoi() returns 0
 * on garbage, which silently matches a valid menu choice of 0 — this
 * is the strict replacement.
 */
int parse_int_strict(const char *s, int *out);

/*
 * safe_strcpy: bounded copy that always null-terminates. dst_sz is the
 * total size of dst, including the terminator (use sizeof(dst) for arrays).
 * Returns 1 if src fit, 0 if it was truncated.
 */
int safe_strcpy(char *dst, const char *src, size_t dst_sz);

#define BUFFER_SIZE 1024
#define HASHKEY     "$6$saltsalt$"

/* Auto-incrementing-ID bases. Each *_utils.c walks the relevant file at
 * write time, finds the current max, and assigns max+1. The bases here
 * are the lower bound used when the file is empty.
 *
 * Customer account ids start at 1000 to make them visually distinct
 * from the small user-table ids (1, 2, …). Loan ids start at 5000 for
 * the same reason. Don't lower these — existing data files reference
 * the larger ranges. */
#define CUSTOMER_ID_BASE 1000
#define LOAN_ID_BASE     5000

/* Hard cap so a corrupted file with a near-INT_MAX id can't wrap us
 * back to negative on the next increment. INT_MAX - 1024 leaves a
 * comfortable buffer for the next 1024 new records. */
#define MAX_AUTO_ID      (2147483647 - 1024)

#define MAX_NAME    50
#define MAX_PASS    50
#define MAX_ROLE    30
#define MAX_TXN_TYPE 12
#define MAX_FEEDBACK 512 
#define MAX_STATUS   20
#define MAX_ASSIGN   50 

typedef struct {
    int id;
    char username[MAX_NAME];
    char password[MAX_PASS];
    int isActive;
} Admin;

typedef struct {
    int id;
    char username[MAX_NAME];
    char password[MAX_PASS];
    char role[MAX_ROLE]; 
    int isActive;
} User;


typedef struct {
    int id;                
    int userId;            
    double balance;
    int isActive;          
} Customer; 

typedef struct {
    time_t timestamp;
    int accountId;             
    char type[MAX_TXN_TYPE];    
    double amount;
    int relatedAccountId;       
} Transaction;


typedef struct {
    int userId;     
    int fd;     
    pid_t pid;      
} Session;

typedef struct {
    int fd;
    off_t start;
    off_t len;
} UserLockInfo; 

typedef struct {
    time_t timestamp;
    int userId;
    char message[MAX_FEEDBACK];
} Feedback;

typedef struct {
    int loanId;
    int accountId;     
    double amount;
    time_t timestamp;
    char status[MAX_STATUS];     
    int employeeId;
} Loan;
#endif 