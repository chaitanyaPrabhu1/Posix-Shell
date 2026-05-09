#include "shell.h"

std::string g_home_dir;
std::string g_username;
std::string g_hostname;
std::string g_cwd;
pid_t g_fg_pid = -1;
std::vector<pid_t> g_bg_pids;
