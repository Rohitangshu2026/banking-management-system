#include "../include/employee.h"
#include "../include/employee_utils.h"
#include "../include/common.h"
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <fcntl.h>

int employeeMenu(int sock) {
    User currentEmployee;
    memset(&currentEmployee, 0, sizeof(User));

    char buffer[128];
    int session_fd = -1; 

    write(sock, "Enter username: ", 16);
    if (readLine(sock, currentEmployee.username, sizeof(currentEmployee.username)) <= 0) return -1; 

    write(sock, "Enter password: ", 16);
    if (readLine(sock, currentEmployee.password, sizeof(currentEmployee.password)) <= 0) return -1; 

    int result = validateEmployee(currentEmployee.username, currentEmployee.password); 

    if (result == -2) {
        write(sock, "Employee already logged in from another terminal.\n", 53);
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
            if (strcmp(u.username, currentEmployee.username) == 0) {
                currentEmployee.id = u.id;
                break;
            }
        }
        close(fd_user);
    }
    
    write(sock, "Login successful!\n", 18);


    int running = 1;
    while (running) {
        const char *menu =
            "\n===== EMPLOYEE MENU =====\n"
            "1. Add New Customer\n"
            "2. Modify Customer Details\n"
            "3. Process Assigned Loan (Approve/Reject)\n" 
            "4. View Assigned Loan Applications\n"      
            "5. View Customer Transactions\n"             
            "6. Change Password\n"                       
            "7. Logout\n"                                
            "Enter your choice: ";

        write(sock, menu, strlen(menu));

        memset(buffer, 0, sizeof(buffer));

        if (readLine(sock, buffer, sizeof(buffer)) <= 0) {
            logoutEmployee(session_fd); 
            break;
        }

        int choice;
        if (!parse_int_strict(buffer, &choice)) {
            write(sock, "Invalid choice. Please enter a number.\n", 39);
            continue;
        }

        switch (choice) {
            case 1:
                addNewCustomer(sock);   
                break;
            case 2:
                modifyCustomerDetails(sock); 
                break;
            case 3: 
                processLoan(sock, currentEmployee.id); 
                break;
            case 4:
                viewAssignedLoans(sock, currentEmployee.id);
                break;
            case 5:
                viewCustomerTransactions(sock);     
                break;
            case 6:
                changeEmployeePassword(sock, currentEmployee.id);   
                break;
            case 7: 
                logoutEmployee(session_fd); 
                write(sock, "Logged out successfully.\n", 27);
                running = 0;
                break;
            default:
                write(sock, "Invalid choice. Please enter 1–7.\n", 35); 
                break;
        }
    }
    return -1; 
}