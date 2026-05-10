#include "shell.h"
#include <functional>

void builtin_pinfo(const std::vector<std::string>& args) {
    pid_t pid = getpid();
    if (args.size() >= 2) {
        try { pid = std::stoi(args[1]); }
        catch (...) { std::cerr << "pinfo: invalid pid\n"; return; }
    }

    std::string stat_path = "/proc/" + std::to_string(pid) + "/stat";
    std::ifstream stat_f(stat_path);
    if (!stat_f) { std::cerr << "pinfo: no such process\n"; return; }

    std::string line;
    std::getline(stat_f, line);
    stat_f.close();

    size_t rp = line.rfind(')');
    if (rp == std::string::npos) { std::cerr << "pinfo: parse error\n"; return; }
    std::string rest = line.substr(rp + 2);
    std::istringstream ss(rest);
    std::vector<std::string> fields;
    std::string tok;
    while (ss >> tok) fields.push_back(tok);

    char state = '?';
    if (!fields.empty()) state = fields[0][0];

    long long vsize = 0;
    if (fields.size() > 20) {
        try { vsize = std::stoll(fields[20]); } catch (...) {}
    }
    vsize /= 1024;

    std::string status_path = "/proc/" + std::to_string(pid) + "/status";
    std::ifstream status_f(status_path);
    std::string fg_str = "";
    if (status_f) {
        std::string sl;
        while (std::getline(status_f, sl)) {
            if (sl.substr(0, 6) == "State:") {
                for (char c : sl) {
                    if (c == 'R' || c == 'S' || c == 'Z' || c == 'T' || c == 'D') {
                        state = c; break;
                    }
                }
            }
        }
    }

    int fg_pgrp = tcgetpgrp(STDIN_FILENO);
    std::string state_str(1, state);
    pid_t proc_pgrp = getpgid(pid);
    if (fg_pgrp != -1 && proc_pgrp == fg_pgrp) state_str += "+";

    std::string exe_path = "/proc/" + std::to_string(pid) + "/exe";
    char exebuf[4096];
    ssize_t exelen = readlink(exe_path.c_str(), exebuf, sizeof(exebuf)-1);
    std::string exe = "?";
    if (exelen > 0) {
        exebuf[exelen] = '\0';
        exe = exebuf;
        if (exe.size() > g_home_dir.size() &&
            exe.substr(0, g_home_dir.size()) == g_home_dir) {
            exe = "~" + exe.substr(g_home_dir.size());
        }
    }

    std::cout << "Process Status -- " << state_str << "\n";
    std::cout << "memory -- " << vsize << " {Virtual Memory}\n";
    std::cout << "Executable Path -- " << exe << "\n";
}

void builtin_search(const std::vector<std::string>& args) {
    if (args.size() < 2) { std::cerr << "search: missing argument\n"; return; }
    std::string target = args[1];

    std::function<bool(const std::string&)> dfs = [&](const std::string& dir) -> bool {
        DIR* d = opendir(dir.c_str());
        if (!d) return false;
        struct dirent* de;
        while ((de = readdir(d)) != nullptr) {
            std::string name = de->d_name;
            if (name == "." || name == "..") continue;
            if (name == target) { closedir(d); return true; }
            if (de->d_type == DT_DIR) {
                if (dfs(dir + "/" + name)) { closedir(d); return true; }
            } else {
                struct stat st;
                std::string fp = dir + "/" + name;
                if (lstat(fp.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
                    if (dfs(fp)) { closedir(d); return true; }
                }
            }
        }
        closedir(d);
        return false;
    };

    std::cout << (dfs(g_cwd) ? "True" : "False") << "\n";
}
