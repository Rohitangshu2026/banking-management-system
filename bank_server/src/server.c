#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>

#include "../include/handler.h"
#include "../include/server.h"

volatile sig_atomic_t keepRunning = 1;

void sigchld_handler(int signo) {
    (void)signo;
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

void sigint_handler(int signo) {
    (void)signo;
    keepRunning = 0;
    printf("\n[SERVER] Caught SIGINT — shutting down cleanly...\n");
    fflush(stdout);
}

int main(void) {
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    struct sigaction sa_int, sa_chld;
    sa_int.sa_handler = sigint_handler;
    sigemptyset(&sa_int.sa_mask);
    sa_int.sa_flags = 0;                
    sigaction(SIGINT, &sa_int, NULL);
    sa_chld.sa_handler = sigchld_handler;
    sigemptyset(&sa_chld.sa_mask);
    sa_chld.sa_flags = SA_RESTART;      
    sigaction(SIGCHLD, &sa_chld, NULL);
    signal(SIGPIPE, SIG_IGN);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("bind");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 5) == -1) {
        perror("listen");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("[SERVER] Listening on port %d...\n", PORT);
    fflush(stdout);

    while (keepRunning) {
        client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd == -1) {
            if (!keepRunning) break;  
            if (errno == EINTR) continue; 
            perror("accept");
            continue;
        }

        printf("[+] Connection accepted from %s:%d\n",
               inet_ntoa(client_addr.sin_addr),
               ntohs(client_addr.sin_port));
        fflush(stdout);

        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            close(client_fd);
            continue;
        }

        if (pid == 0) {
            close(server_fd);
            printf("[+] New client connected (pid=%d)\n", getpid());
            fflush(stdout);

            handle_client(client_fd);

            printf("[-] Client disconnected (pid=%d)\n", getpid());
            fflush(stdout);

            close(client_fd);
            _exit(0);
        } 
        else {
            close(client_fd);
        }
    }

    close(server_fd);
    printf("[SERVER] Shutdown complete.\n");
    fflush(stdout);

    return 0;
}
