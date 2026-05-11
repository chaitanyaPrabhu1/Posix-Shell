#include "shell.h"

void handle_sigchld(int) {
    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        auto it = std::find(g_bg_pids.begin(), g_bg_pids.end(), pid);
        if (it != g_bg_pids.end()) {
            g_bg_pids.erase(it);
        }
    }
}

void handle_sigtstp(int) {
    if (g_fg_pid > 0) {
        kill(g_fg_pid, SIGTSTP);
        g_bg_pids.push_back(g_fg_pid);
        g_fg_pid = -1;
    }
}

void handle_sigint(int) {
    if (g_fg_pid > 0) {
        kill(g_fg_pid, SIGINT);
    }
}
