#include "shell.h"

std::vector<std::string> tokenize(const std::string& s) {
    std::vector<std::string> tokens;
    std::string cur;
    for (size_t i = 0; i < s.size(); i++) {
        char c = s[i];
        if (c == ' ' || c == '\t') {
            if (!cur.empty()) { tokens.push_back(cur); cur.clear(); }
        } else {
            cur += c;
        }
    }
    if (!cur.empty()) tokens.push_back(cur);
    return tokens;
}

std::vector<std::string> split_by(const std::string& s, char delim) {
    std::vector<std::string> parts;
    std::string cur;
    for (char c : s) {
        if (c == delim) {
            parts.push_back(cur);
            cur.clear();
        } else {
            cur += c;
        }
    }
    parts.push_back(cur);
    return parts;
}

Command parse_command(const std::string& raw) {
    Command cmd;
    std::vector<std::string> tokens = tokenize(raw);

    if (!tokens.empty() && tokens.back() == "&") {
        cmd.background = true;
        tokens.pop_back();
    } else if (!tokens.empty() && tokens.back().size() > 1 && tokens.back().back() == '&') {
        cmd.background = true;
        tokens.back().pop_back();
    }

    for (size_t i = 0; i < tokens.size(); i++) {
        if (tokens[i] == "<") {
            if (i + 1 < tokens.size()) { cmd.infile = tokens[++i]; }
        } else if (tokens[i] == ">>") {
            if (i + 1 < tokens.size()) { cmd.outfile = tokens[++i]; cmd.append = true; }
        } else if (tokens[i] == ">") {
            if (i + 1 < tokens.size()) { cmd.outfile = tokens[++i]; cmd.append = false; }
        } else {
            cmd.args.push_back(tokens[i]);
        }
    }
    return cmd;
}

std::string get_display_dir() {
    if (g_cwd == g_home_dir) return "~";
    if (g_cwd.size() > g_home_dir.size() &&
        g_cwd.substr(0, g_home_dir.size()) == g_home_dir &&
        g_cwd[g_home_dir.size()] == '/') {
        return "~" + g_cwd.substr(g_home_dir.size());
    }
    return g_cwd;
}

void update_cwd() {
    char buf[4096];
    if (getcwd(buf, sizeof(buf))) g_cwd = buf;
}

void print_prompt() {
    std::cout << "<" << g_username << "@" << g_hostname << ":" << get_display_dir() << "> " << std::flush;
}
