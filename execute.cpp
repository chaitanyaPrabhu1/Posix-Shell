#include "shell.h"

static void apply_redirects(const Command& cmd) {
    if (!cmd.infile.empty()) {
        int fd = open(cmd.infile.c_str(), O_RDONLY);
        if (fd < 0) { perror(cmd.infile.c_str()); exit(1); }
        dup2(fd, STDIN_FILENO);
        close(fd);
    }
    if (!cmd.outfile.empty()) {
        int flags = O_WRONLY | O_CREAT | (cmd.append ? O_APPEND : O_TRUNC);
        int fd = open(cmd.outfile.c_str(), flags, 0644);
        if (fd < 0) { perror(cmd.outfile.c_str()); exit(1); }
        dup2(fd, STDOUT_FILENO);
        close(fd);
    }
}

static bool is_builtin(const std::string& name) {
    return name == "cd" || name == "echo" || name == "pwd" ||
           name == "ls" || name == "pinfo" || name == "search" ||
           name == "history";
}

static void run_builtin(const std::vector<std::string>& args) {
    if (args.empty()) return;
    if (args[0] == "cd")      builtin_cd(args);
    else if (args[0] == "echo")    builtin_echo(args);
    else if (args[0] == "pwd")     builtin_pwd(args);
    else if (args[0] == "ls")      builtin_ls(args);
    else if (args[0] == "pinfo")   builtin_pinfo(args);
    else if (args[0] == "search")  builtin_search(args);
    else if (args[0] == "history") builtin_history_cmd(args);
}

static void exec_cmd(const Command& cmd) {
    if (cmd.args.empty()) return;

    if (is_builtin(cmd.args[0])) {
        int saved_in = -1, saved_out = -1;
        if (!cmd.infile.empty()) {
            saved_in = dup(STDIN_FILENO);
            int fd = open(cmd.infile.c_str(), O_RDONLY);
            if (fd < 0) { perror(cmd.infile.c_str()); return; }
            dup2(fd, STDIN_FILENO); close(fd);
        }
        if (!cmd.outfile.empty()) {
            saved_out = dup(STDOUT_FILENO);
            int flags = O_WRONLY | O_CREAT | (cmd.append ? O_APPEND : O_TRUNC);
            int fd = open(cmd.outfile.c_str(), flags, 0644);
            if (fd < 0) { perror(cmd.outfile.c_str()); return; }
            dup2(fd, STDOUT_FILENO); close(fd);
        }
        run_builtin(cmd.args);
        if (saved_in >= 0)  { dup2(saved_in, STDIN_FILENO);  close(saved_in); }
        if (saved_out >= 0) { dup2(saved_out, STDOUT_FILENO); close(saved_out); }
        return;
    }

    pid_t pid = fork();
    if (pid < 0) { perror("fork"); return; }
    if (pid == 0) {
        setpgid(0, 0);
        apply_redirects(cmd);
        std::vector<char*> argv;
        for (auto& a : cmd.args) argv.push_back(const_cast<char*>(a.c_str()));
        argv.push_back(nullptr);
        execvp(argv[0], argv.data());
        perror(argv[0]);
        exit(1);
    }

    setpgid(pid, pid);
    if (cmd.background) {
        g_bg_pids.push_back(pid);
        std::cout << "[" << pid << "]\n";
    } else {
        g_fg_pid = pid;
        tcsetpgrp(STDIN_FILENO, pid);
        int status;
        waitpid(pid, &status, WUNTRACED);
        tcsetpgrp(STDIN_FILENO, getpgrp());
        g_fg_pid = -1;
    }
}

void execute_pipeline(const std::vector<std::string>& cmds) {
    if (cmds.size() == 1) {
        Command c = parse_command(cmds[0]);
        exec_cmd(c);
        return;
    }

    int n = cmds.size();
    std::vector<int[2]> pipes(n - 1);
    for (int i = 0; i < n - 1; i++) {
        if (pipe(pipes[i]) < 0) { perror("pipe"); return; }
    }

    std::vector<pid_t> pids;
    for (int i = 0; i < n; i++) {
        Command c = parse_command(cmds[i]);

        pid_t pid = fork();
        if (pid < 0) { perror("fork"); return; }
        if (pid == 0) {
            if (i > 0) { dup2(pipes[i-1][0], STDIN_FILENO); }
            if (i < n-1) { dup2(pipes[i][1], STDOUT_FILENO); }
            for (int j = 0; j < n-1; j++) { close(pipes[j][0]); close(pipes[j][1]); }
            apply_redirects(c);
            if (is_builtin(c.args[0])) {
                run_builtin(c.args);
                exit(0);
            }
            std::vector<char*> argv;
            for (auto& a : c.args) argv.push_back(const_cast<char*>(a.c_str()));
            argv.push_back(nullptr);
            execvp(argv[0], argv.data());
            perror(argv[0]);
            exit(1);
        }
        pids.push_back(pid);
        if (i > 0) { close(pipes[i-1][0]); close(pipes[i-1][1]); }
    }
    if (n >= 2) { close(pipes[n-2][0]); close(pipes[n-2][1]); }

    Command last = parse_command(cmds[n-1]);
    bool bg = last.background;
    if (!bg) {
        for (pid_t p : pids) {
            g_fg_pid = p;
            int status;
            waitpid(p, &status, WUNTRACED);
        }
        g_fg_pid = -1;
    } else {
        for (pid_t p : pids) g_bg_pids.push_back(p);
    }
}

void execute_command(const std::string& cmd_str, bool background) {
    Command c = parse_command(cmd_str);
    c.background = c.background || background;
    exec_cmd(c);
}

void process_line(const std::string& raw_line) {
    std::string line = raw_line;
    while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) line.pop_back();
    if (line.empty()) return;

    history_add(line);

    std::vector<std::string> semicolons = split_by(line, ';');
    for (auto& segment : semicolons) {
        if (segment.empty()) continue;
        std::vector<std::string> pipe_parts = split_by(segment, '|');
        for (auto& p : pipe_parts) {
            std::string t = p;
            size_t s = t.find_first_not_of(" \t");
            if (s != std::string::npos) t = t.substr(s);
            size_t e = t.find_last_not_of(" \t");
            if (e != std::string::npos) t = t.substr(0, e+1);
        }
        if (pipe_parts.size() == 1) {
            std::string s = segment;
            size_t b = s.find_first_not_of(" \t");
            if (b == std::string::npos) continue;
            s = s.substr(b);
            Command c = parse_command(s);
            if (c.args.empty()) continue;
            if (c.args[0] == "exit") { history_save(); exit(0); }
            exec_cmd(c);
        } else {
            std::vector<std::string> trimmed;
            for (auto& p : pipe_parts) {
                std::string t = p;
                size_t b = t.find_first_not_of(" \t");
                if (b == std::string::npos) continue;
                t = t.substr(b);
                size_t e = t.find_last_not_of(" \t");
                if (e != std::string::npos) t = t.substr(0, e+1);
                if (!t.empty()) trimmed.push_back(t);
            }
            execute_pipeline(trimmed);
        }
    }
}
