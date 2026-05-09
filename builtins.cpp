#include "shell.h"
#include <grp.h>

static std::string perm_string(mode_t mode, bool is_dir, bool is_link) {
    std::string s(10, '-');
    if (is_link)      s[0] = 'l';
    else if (is_dir)  s[0] = 'd';
    else if (S_ISCHR(mode)) s[0] = 'c';
    else if (S_ISBLK(mode)) s[0] = 'b';
    else if (S_ISFIFO(mode)) s[0] = 'p';
    else if (S_ISSOCK(mode)) s[0] = 's';
    s[1] = (mode & S_IRUSR) ? 'r' : '-';
    s[2] = (mode & S_IWUSR) ? 'w' : '-';
    s[3] = (mode & S_IXUSR) ? 'x' : '-';
    s[4] = (mode & S_IRGRP) ? 'r' : '-';
    s[5] = (mode & S_IWGRP) ? 'w' : '-';
    s[6] = (mode & S_IXGRP) ? 'x' : '-';
    s[7] = (mode & S_IROTH) ? 'r' : '-';
    s[8] = (mode & S_IWOTH) ? 'w' : '-';
    s[9] = (mode & S_IXOTH) ? 'x' : '-';
    if (mode & S_ISUID) s[3] = (mode & S_IXUSR) ? 's' : 'S';
    if (mode & S_ISGID) s[6] = (mode & S_IXGRP) ? 's' : 'S';
    if (mode & S_ISVTX) s[9] = (mode & S_IXOTH) ? 't' : 'T';
    return s;
}

static bool cmp_name(const std::string& a, const std::string& b) {
    std::string la = a, lb = b;
    for (char& c : la) c = tolower(c);
    for (char& c : lb) c = tolower(c);
    return la < lb;
}

static void ls_dir(const std::string& path, bool show_all, bool long_fmt) {
    DIR* d = opendir(path.c_str());
    if (!d) { perror("ls"); return; }

    std::vector<std::string> entries;
    struct dirent* de;
    while ((de = readdir(d)) != nullptr) {
        std::string name = de->d_name;
        if (!show_all && name[0] == '.') continue;
        entries.push_back(name);
    }
    closedir(d);
    std::sort(entries.begin(), entries.end(), cmp_name);

    if (!long_fmt) {
        for (auto& e : entries) std::cout << e << "\n";
        return;
    }

    long long total_blocks = 0;
    struct stat st;
    for (auto& e : entries) {
        std::string fp = path + "/" + e;
        if (lstat(fp.c_str(), &st) == 0) total_blocks += st.st_blocks;
    }
    std::cout << "total " << total_blocks / 2 << "\n";

    for (auto& e : entries) {
        std::string fp = path + "/" + e;
        if (lstat(fp.c_str(), &st) != 0) { perror("lstat"); continue; }
        bool is_link = S_ISLNK(st.st_mode);
        bool is_dir  = S_ISDIR(st.st_mode);
        std::string perms = perm_string(st.st_mode, is_dir, is_link);

        struct passwd* pw = getpwuid(st.st_uid);
        struct group*  gr = getgrgid(st.st_gid);
        std::string owner = pw ? pw->pw_name : std::to_string(st.st_uid);
        std::string group = gr ? gr->gr_name : std::to_string(st.st_gid);

        char timebuf[64];
        struct tm* tm_info = localtime(&st.st_mtime);
        strftime(timebuf, sizeof(timebuf), "%b %e %H:%M", tm_info);

        printf("%s %2lu %-8s %-8s %8lld %s %s",
            perms.c_str(), (unsigned long)st.st_nlink,
            owner.c_str(), group.c_str(),
            (long long)st.st_size, timebuf, e.c_str());

        if (is_link) {
            char lbuf[4096];
            ssize_t len = readlink(fp.c_str(), lbuf, sizeof(lbuf)-1);
            if (len > 0) { lbuf[len] = '\0'; printf(" -> %s", lbuf); }
        }
        printf("\n");
    }
}

static void ls_file(const std::string& path, bool long_fmt) {
    struct stat st;
    if (lstat(path.c_str(), &st) != 0) { perror("ls"); return; }
    if (!long_fmt) {
        size_t pos = path.rfind('/');
        std::cout << (pos == std::string::npos ? path : path.substr(pos+1)) << "\n";
        return;
    }
    bool is_link = S_ISLNK(st.st_mode);
    bool is_dir  = S_ISDIR(st.st_mode);
    std::string perms = perm_string(st.st_mode, is_dir, is_link);
    struct passwd* pw = getpwuid(st.st_uid);
    struct group*  gr = getgrgid(st.st_gid);
    std::string owner = pw ? pw->pw_name : std::to_string(st.st_uid);
    std::string group = gr ? gr->gr_name : std::to_string(st.st_gid);
    char timebuf[64];
    struct tm* tm_info = localtime(&st.st_mtime);
    strftime(timebuf, sizeof(timebuf), "%b %e %H:%M", tm_info);
    size_t pos = path.rfind('/');
    std::string fname = (pos == std::string::npos ? path : path.substr(pos+1));
    printf("%s %2lu %-8s %-8s %8lld %s %s\n",
        perms.c_str(), (unsigned long)st.st_nlink,
        owner.c_str(), group.c_str(),
        (long long)st.st_size, timebuf, fname.c_str());
}

void builtin_ls(const std::vector<std::string>& args) {
    bool show_all = false, long_fmt = false;
    std::vector<std::string> paths;

    for (size_t i = 1; i < args.size(); i++) {
        const std::string& a = args[i];
        if (!a.empty() && a[0] == '-') {
            for (size_t j = 1; j < a.size(); j++) {
                if (a[j] == 'a') show_all = true;
                else if (a[j] == 'l') long_fmt = true;
            }
        } else {
            paths.push_back(a);
        }
    }

    if (paths.empty()) paths.push_back(".");

    for (auto& p : paths) {
        std::string resolved = p;
        if (p == "~") resolved = g_home_dir;
        else if (p == ".") resolved = g_cwd;
        else if (p == "..") {
            std::string tmp = g_cwd;
            size_t pos = tmp.rfind('/');
            resolved = (pos == 0) ? "/" : tmp.substr(0, pos);
        } else if (p[0] != '/') {
            resolved = g_cwd + "/" + p;
        }

        struct stat st;
        if (stat(resolved.c_str(), &st) != 0) { perror("ls"); continue; }
        if (paths.size() > 1) std::cout << p << ":\n";
        if (S_ISDIR(st.st_mode)) ls_dir(resolved, show_all, long_fmt);
        else ls_file(resolved, long_fmt);
    }
}

void builtin_cd(const std::vector<std::string>& args) {
    static std::string prev_dir = "";

    if (args.size() > 2) { std::cerr << "Invalid arguments\n"; return; }

    std::string target;
    if (args.size() == 1 || args[1] == "~") {
        target = g_home_dir;
    } else if (args[1] == "-") {
        if (prev_dir.empty()) { std::cerr << "cd: OLDPWD not set\n"; return; }
        target = prev_dir;
        std::cout << target << "\n";
    } else if (args[1] == ".") {
        return;
    } else if (args[1] == "..") {
        std::string tmp = g_cwd;
        size_t pos = tmp.rfind('/');
        target = (pos == 0) ? "/" : tmp.substr(0, pos);
    } else {
        if (args[1][0] == '/') target = args[1];
        else target = g_cwd + "/" + args[1];
    }

    std::string old = g_cwd;
    if (chdir(target.c_str()) != 0) { perror("cd"); return; }
    prev_dir = old;
    update_cwd();
}

void builtin_pwd(const std::vector<std::string>&) {
    std::cout << g_cwd << "\n";
}

void builtin_echo(const std::vector<std::string>& args) {
    for (size_t i = 1; i < args.size(); i++) {
        if (i > 1) std::cout << " ";
        std::cout << args[i];
    }
    std::cout << "\n";
}
