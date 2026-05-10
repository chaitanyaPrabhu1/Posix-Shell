#include "shell.h"

static struct termios g_orig_termios;

static void enable_raw_mode() {
    tcgetattr(STDIN_FILENO, &g_orig_termios);
    struct termios raw = g_orig_termios;
    raw.c_lflag &= ~(ICANON | ECHO | ISIG);
    raw.c_iflag &= ~(IXON | ICRNL);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

static void disable_raw_mode() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &g_orig_termios);
}

static std::vector<std::string> get_file_completions(const std::string& prefix) {
    std::vector<std::string> results;
    DIR* d = opendir(".");
    if (!d) return results;
    struct dirent* de;
    while ((de = readdir(d)) != nullptr) {
        std::string name = de->d_name;
        if (name == "." || name == "..") continue;
        if (prefix.empty() || name.substr(0, prefix.size()) == prefix)
            results.push_back(name);
    }
    closedir(d);
    std::sort(results.begin(), results.end());
    return results;
}

static std::vector<std::string> get_cmd_completions(const std::string& prefix) {
    std::vector<std::string> results;
    std::vector<std::string> builtins = {"cd","echo","pwd","ls","pinfo","search","history","exit"};
    for (auto& b : builtins)
        if (prefix.empty() || b.substr(0, prefix.size()) == prefix)
            results.push_back(b);

    const char* path_env = getenv("PATH");
    if (!path_env) return results;
    std::vector<std::string> dirs = split_by(path_env, ':');
    std::vector<std::string> cmds;
    for (auto& dir : dirs) {
        DIR* d = opendir(dir.c_str());
        if (!d) continue;
        struct dirent* de;
        while ((de = readdir(d)) != nullptr) {
            std::string name = de->d_name;
            if (prefix.empty() || name.substr(0, prefix.size()) == prefix) {
                std::string fp = dir + "/" + name;
                struct stat st;
                if (stat(fp.c_str(), &st) == 0 && (st.st_mode & S_IXUSR))
                    cmds.push_back(name);
            }
        }
        closedir(d);
    }
    std::sort(cmds.begin(), cmds.end());
    cmds.erase(std::unique(cmds.begin(), cmds.end()), cmds.end());
    for (auto& c : cmds) results.push_back(c);
    return results;
}

static std::string common_prefix(const std::vector<std::string>& v) {
    if (v.empty()) return "";
    std::string pref = v[0];
    for (auto& s : v) {
        size_t i = 0;
        while (i < pref.size() && i < s.size() && pref[i] == s[i]) i++;
        pref = pref.substr(0, i);
    }
    return pref;
}

static void redraw_line(const std::string& buf, size_t cursor) {
    write(STDOUT_FILENO, "\r\033[K", 4);
    print_prompt();
    write(STDOUT_FILENO, buf.c_str(), buf.size());
    if (cursor < buf.size()) {
        std::string mv = "\033[" + std::to_string(buf.size() - cursor) + "D";
        write(STDOUT_FILENO, mv.c_str(), mv.size());
    }
}

std::string read_line_raw() {
    enable_raw_mode();

    std::string buf;
    size_t cursor = 0;
    std::vector<std::string> hist = history_get();
    int hist_idx = (int)hist.size();
    std::string saved_buf;

    print_prompt();

    while (true) {
        char c;
        ssize_t n = read(STDIN_FILENO, &c, 1);
        if (n <= 0) {
            disable_raw_mode();
            if (n == 0 || (n < 0 && errno == EINTR)) continue;
            write(STDOUT_FILENO, "\n", 1);
            return "exit";
        }

        if (c == 4) {
            write(STDOUT_FILENO, "\n", 1);
            disable_raw_mode();
            return "exit";
        }

        if (c == '\n' || c == '\r') {
            write(STDOUT_FILENO, "\n", 1);
            disable_raw_mode();
            return buf;
        }

        if (c == 127 || c == 8) {
            if (cursor > 0) {
                buf.erase(cursor-1, 1);
                cursor--;
                redraw_line(buf, cursor);
            }
            continue;
        }

        if (c == 27) {
            char seq[3];
            if (read(STDIN_FILENO, &seq[0], 1) <= 0) continue;
            if (seq[0] == '[') {
                if (read(STDIN_FILENO, &seq[1], 1) <= 0) continue;
                if (seq[1] == 'A') {
                    if (hist_idx == (int)hist.size()) saved_buf = buf;
                    if (hist_idx > 0) {
                        hist_idx--;
                        buf = hist[hist_idx];
                        cursor = buf.size();
                        redraw_line(buf, cursor);
                    }
                } else if (seq[1] == 'B') {
                    if (hist_idx < (int)hist.size()) {
                        hist_idx++;
                        buf = (hist_idx == (int)hist.size()) ? saved_buf : hist[hist_idx];
                        cursor = buf.size();
                        redraw_line(buf, cursor);
                    }
                } else if (seq[1] == 'C') {
                    if (cursor < buf.size()) { cursor++; write(STDOUT_FILENO, "\033[C", 3); }
                } else if (seq[1] == 'D') {
                    if (cursor > 0) { cursor--; write(STDOUT_FILENO, "\033[D", 3); }
                }
            }
            continue;
        }

        if (c == '\t') {
            std::vector<std::string> tokens = tokenize(buf);
            bool is_cmd = tokens.empty() || (tokens.size() == 1 && cursor <= buf.size());
            std::string last_token = "";
            if (!tokens.empty()) {
                size_t last_space = buf.rfind(' ');
                if (last_space == std::string::npos) last_token = buf;
                else last_token = buf.substr(last_space + 1);
                if (!buf.empty() && buf.back() == ' ') last_token = "";
                is_cmd = (last_space == std::string::npos && buf.find(' ') == std::string::npos);
            } else {
                is_cmd = true;
            }

            std::vector<std::string> matches;
            if (is_cmd) matches = get_cmd_completions(last_token);
            else         matches = get_file_completions(last_token);

            if (matches.empty()) continue;

            if (matches.size() == 1) {
                std::string suffix = matches[0].substr(last_token.size());
                buf.insert(cursor, suffix);
                cursor += suffix.size();
                buf.insert(cursor, " ");
                cursor++;
                redraw_line(buf, cursor);
            } else {
                std::string cp = common_prefix(matches);
                if (cp.size() > last_token.size()) {
                    std::string suffix = cp.substr(last_token.size());
                    buf.insert(cursor, suffix);
                    cursor += suffix.size();
                    redraw_line(buf, cursor);
                } else {
                    write(STDOUT_FILENO, "\n", 1);
                    for (size_t i = 0; i < matches.size(); i++) {
                        std::cout << matches[i];
                        if (i + 1 < matches.size()) std::cout << "  ";
                    }
                    write(STDOUT_FILENO, "\n", 1);
                    redraw_line(buf, cursor);
                }
            }
            continue;
        }

        if (c >= 32) {
            buf.insert(cursor, 1, c);
            cursor++;
            if (cursor == buf.size()) {
                write(STDOUT_FILENO, &c, 1);
            } else {
                redraw_line(buf, cursor);
            }
        }
    }
}
