#include"http.hpp"
#define WWWROOT "./HTTPRESOURC"

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
    HttpServer server(30,8081,10);
    server.SetThreadCount(3);
    server.SetBaseDir(WWWROOT);
    server.Get("/hello", Hello);
    server.Post("/login", Login);
    //注意：路由是正则表达式，"." 需要转义
    server.Put("/1234\\.txt", PutFile);
    server.Delete("/1234\\.txt", DelFile);
    server.Listen();
    return 0;
}