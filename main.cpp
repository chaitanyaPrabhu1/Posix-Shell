#include "shell.h"

int main() {
    struct passwd* pw = getpwuid(getuid());
    g_username = pw ? pw->pw_name : "user";
    g_home_dir = pw ? pw->pw_dir : "/tmp";

    char hbuf[256];
    gethostname(hbuf, sizeof(hbuf));
    g_hostname = hbuf;

    update_cwd();

    history_load();

    struct sigaction sa_chld{};
    sa_chld.sa_handler = handle_sigchld;
    sa_chld.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigemptyset(&sa_chld.sa_mask);
    sigaction(SIGCHLD, &sa_chld, nullptr);

    struct sigaction sa_tstp{};
    sa_tstp.sa_handler = handle_sigtstp;
    sigemptyset(&sa_tstp.sa_mask);
    sigaction(SIGTSTP, &sa_tstp, nullptr);

    struct sigaction sa_int{};
    sa_int.sa_handler = handle_sigint;
    sigemptyset(&sa_int.sa_mask);
    sigaction(SIGINT, &sa_int, nullptr);

    struct sigaction sa_ign{};
    sa_ign.sa_handler = SIG_IGN;
    sigemptyset(&sa_ign.sa_mask);
    sigaction(SIGTTOU, &sa_ign, nullptr);
    sigaction(SIGTTIN, &sa_ign, nullptr);

    while (true) {
        std::string line = read_line_raw();
        if (line == "exit") {
            history_save();
            break;
        }
        if (!line.empty()) process_line(line);
    }

    return 0;
}
