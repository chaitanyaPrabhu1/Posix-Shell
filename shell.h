#pragma once
#include <iostream>
#include <string>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <cerrno>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <dirent.h>
#include <pwd.h>
#include <termios.h>
#include <algorithm>
#include <fstream>
#include <sstream>

extern std::string g_home_dir;
extern std::string g_username;
extern std::string g_hostname;
extern std::string g_cwd;
extern pid_t g_fg_pid;
extern std::vector<pid_t> g_bg_pids;

std::string get_display_dir();
void update_cwd();
void print_prompt();

void builtin_cd(const std::vector<std::string>& args);
void builtin_echo(const std::vector<std::string>& args);
void builtin_pwd(const std::vector<std::string>& args);
void builtin_ls(const std::vector<std::string>& args);
void builtin_pinfo(const std::vector<std::string>& args);
void builtin_search(const std::vector<std::string>& args);
void builtin_history_cmd(const std::vector<std::string>& args);

void handle_sigchld(int sig);
void handle_sigtstp(int sig);
void handle_sigint(int sig);

void execute_pipeline(const std::vector<std::string>& cmds);
void execute_command(const std::string& cmd_str, bool background);
void process_line(const std::string& line);

void history_load();
void history_save();
void history_add(const std::string& cmd);
std::vector<std::string> history_get();

std::string read_line_raw();

struct Command {
    std::vector<std::string> args;
    std::string infile;
    std::string outfile;
    bool append;
    bool background;
    Command() : append(false), background(false) {}
};

Command parse_command(const std::string& s);
std::vector<std::string> split_by(const std::string& s, char delim);
std::vector<std::string> tokenize(const std::string& s);
