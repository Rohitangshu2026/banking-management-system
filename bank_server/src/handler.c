#include <stdio.h>
#include <stdlib.h>    
#include <string.h>     
#include <unistd.h>      
#include <fcntl.h>       
#include <errno.h>

#include "../include/handler.h"
#include "../include/server.h"
#include "../include/common.h"
#include "../include/admin.h"
#include "../include/admin_utils.h" 
#include "../include/manager.h"
#include "../include/employee.h"
#include "../include/customer.h"

void handle_client(int sock) {
    char buffer[1024];
    UserLockInfo adminLock = { .fd = -1 };
    int session_fd = -1; 
    while (1) {
        if (write(sock, MAINMENU, strlen(MAINMENU)) <= 0) {
            break; 
        }
        int flags = fcntl(sock, F_GETFL, 0);
        if (flags != -1) {
            fcntl(sock, F_SETFL, flags | O_NONBLOCK);
            char tmp[128];
            while (read(sock, tmp, sizeof(tmp)) > 0); 
            fcntl(sock, F_SETFL, flags);
        }
        memset(buffer, 0, sizeof(buffer));
        if (readLine(sock, buffer, sizeof(buffer)) <= 0) {
            break; 
        }
        if (strlen(buffer) == 0) {
            continue; 
        }     
        int choice;
        if (!parse_int_strict(buffer, &choice)) {
            write(sock, "Invalid choice\n", 15);
            continue;
        }
        switch (choice) {
            case 1: 
                session_fd = customerMenu(sock);
                continue;
            case 2: 
                session_fd = employeeMenu(sock);
                continue;
            case 3: 
                session_fd = managerMenu(sock);
                continue;
            case 4: 
                adminMenu(sock, &adminLock);
                continue;
            case 5: 
                write(sock, "Client logging out...\n", 23);
                if (adminLock.fd != -1)
                    unlockAdmin(&adminLock);
                if (session_fd != -1)
                    close(session_fd); 
                
                close(sock);
                _exit(0); 
            default:
                write(sock, "Invalid choice\n", 15);
                break;
        }
    }

    printf("[-] Client disconnected (pid=%d), releasing locks.\n", getpid());
    fflush(stdout);
    if (adminLock.fd != -1)
        unlockAdmin(&adminLock); 
    if (session_fd != -1)
        close(session_fd); 
    close(sock);
    _exit(0); 
}