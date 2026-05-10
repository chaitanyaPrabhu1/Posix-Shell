#include "shell.h"

static const std::string HISTORY_FILE = std::string(getenv("HOME") ? getenv("HOME") : "/tmp") + "/.myshell_history";
static const int HISTORY_MAX = 20;
static const int HISTORY_SHOW = 10;
static std::vector<std::string> g_history;

void history_load() {
    std::ifstream f(HISTORY_FILE);
    if (!f) return;
    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty()) g_history.push_back(line);
    }
    while ((int)g_history.size() > HISTORY_MAX) {
        g_history.erase(g_history.begin());
    }
}

void history_save() {
    std::ofstream f(HISTORY_FILE, std::ios::trunc);
    for (auto& l : g_history) f << l << "\n";
}

void history_add(const std::string& cmd) {
    if (cmd.empty()) return;
    g_history.push_back(cmd);
    while ((int)g_history.size() > HISTORY_MAX) {
        g_history.erase(g_history.begin());
    }
    history_save();
}

std::vector<std::string> history_get() {
    return g_history;
}

void builtin_history_cmd(const std::vector<std::string>& args) {
    int n = HISTORY_SHOW;
    if (args.size() >= 2) {
        try { n = std::stoi(args[1]); } catch (...) { n = HISTORY_SHOW; }
    }

    int start = (int)g_history.size() - n;
    if (start < 0) start = 0;
    for (int i = start; i < (int)g_history.size(); i++) {
        std::cout << g_history[i] << "\n";
    }
}
