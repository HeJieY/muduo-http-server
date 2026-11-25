#include"http.hpp"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fcntl.h>
#include <signal.h>
#include <string>
#include <unistd.h>

#define WWWROOT "./HTTPRESOURC"

// 让服务脱离当前终端会话：daemon() 内部会 fork + setsid，
// 这样关闭 SSH/终端时产生的 SIGHUP 不会杀掉服务器进程。
// 第一个参数 1 表示保留当前工作目录，第二个参数 0 表示先用 /dev/null 接管标准输入输出，
// 随后再把 stdout/stderr 重定向到日志文件，方便排查问题。
static bool BecomeDaemon(const std::string &logPath)
{
    int logFd = open(logPath.c_str(), O_CREAT | O_WRONLY | O_APPEND, 0644);
    if (logFd < 0)
    {
        std::fprintf(stderr, "open log file '%s' failed: %s\n",
                     logPath.c_str(), std::strerror(errno));
        return false;
    }

    if (daemon(1, 0) < 0)
    {
        std::fprintf(stderr, "daemon() failed: %s\n", std::strerror(errno));
        close(logFd);
        return false;
    }

    // daemon 已经把 0/1/2 指到 /dev/null，这里把 stdout/stderr 接回日志文件。
    if (dup2(logFd, STDOUT_FILENO) < 0 || dup2(logFd, STDERR_FILENO) < 0)
    {
        close(logFd);
        return false;
    }
    close(logFd);

    // 日志改成行缓冲，tail -f 能及时看到输出。
    setvbuf(stdout, nullptr, _IOLBF, 0);
    setvbuf(stderr, nullptr, _IONBF, 0);

    // stdin 保持 /dev/null，避免后台进程误读终端。
    int nullFd = open("/dev/null", O_RDONLY);
    if (nullFd >= 0)
    {
        dup2(nullFd, STDIN_FILENO);
        close(nullFd);
    }

    std::fprintf(stderr, "http server daemon started, pid=%d, log=%s\n",
                 static_cast<int>(getpid()), logPath.c_str());
    std::fflush(stderr);
    return true;
}

std::string RequestStr(const HttpRequest &req) {
    std::stringstream ss;
    ss << req._method << " " << req._path << " " << req._version << "\r\n";
    for (auto &it : req._params) {
        ss << it.first << ": " << it.second << "\r\n";
    }
    for (auto &it : req._headers) {
        ss << it.first << ": " << it.second << "\r\n";
    }
    ss << "\r\n";
    ss << req._body;
    return ss.str();
}
void Hello(const HttpRequest &req, HttpResponse *rsp) 
{
    rsp->SetContent(RequestStr(req), "text/plain");
}
void Login(const HttpRequest &req, HttpResponse *rsp) 
{
    rsp->SetContent(RequestStr(req), "text/plain");
}
void PutFile(const HttpRequest &req, HttpResponse *rsp) 
{
    //即使以后把路由正则放宽，也不能允许 ../ 或 NUL 逃出 WWWROOT
    if (Util::ValidPath(req._path) == false)
    {
        rsp->_statu = 403;
        return;
    }
    std::string pathname = WWWROOT + req._path;
    if (Util::WriteFile(pathname, req._body) == false)
    {
        rsp->_statu = 500;
        return;
    }
    rsp->SetContent("OK\n", "text/plain");
}
void DelFile(const HttpRequest &req, HttpResponse *rsp) 
{
    rsp->SetContent(RequestStr(req), "text/plain");
}
int main()
{
    // 忽略 SIGPIPE，避免客户端提前断开时 send 直接杀死进程。
    signal(SIGPIPE, SIG_IGN);
    // 额外忽略 SIGHUP；daemon() 已经 setsid，这里是双保险。
    signal(SIGHUP, SIG_IGN);

    // 必须在 daemon() 之前把资源路径解析成绝对路径。
    // daemon(1, 0) 会保留当前工作目录，但用绝对路径更稳妥，
    // 以后即使改成 daemon(0, 0) 也不会找不到 HTTPRESOURC。
    const std::string wwwroot =
        std::filesystem::absolute(WWWROOT).lexically_normal().string();
    const std::string logPath =
        std::filesystem::absolute("http_server.log").lexically_normal().string();

    if (BecomeDaemon(logPath) == false)
        return 1;

    HttpServer server(30,8081,10);
    server.SetThreadCount(3);
    server.SetBaseDir(wwwroot);
    server.Get("/hello", Hello);
    server.Post("/login", Login);
    //注意：路由是正则表达式，"." 需要转义
    server.Put("/1234\\.txt", PutFile);
    server.Delete("/1234\\.txt", DelFile);
    server.Listen();
    return 0;
}