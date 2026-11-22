#pragma once

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <iostream>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <sys/types.h>
#include <unordered_map>
#include <utility>
#include <vector>
#include <regex>
#include <sys/stat.h>
#include "../source/Server.hpp"

#define DEFALT_TIMEOUT 10

inline const std::unordered_map<int, std::string> _statu_msg = {
    {100,  "Continue"},
    {101,  "Switching Protocol"},
    {102,  "Processing"},
    {103,  "Early Hints"},
    {200,  "OK"},
    {201,  "Created"},
    {202,  "Accepted"},
    {203,  "Non-Authoritative Information"},
    {204,  "No Content"},
    {205,  "Reset Content"},
    {206,  "Partial Content"},
    {207,  "Multi-Status"},
    {208,  "Already Reported"},
    {226,  "IM Used"},
    {300,  "Multiple Choice"},
    {301,  "Moved Permanently"},
    {302,  "Found"},
    {303,  "See Other"},
    {304,  "Not Modified"},
    {305,  "Use Proxy"},
    {306,  "unused"},
    {307,  "Temporary Redirect"},
    {308,  "Permanent Redirect"},
    {400,  "Bad Request"},
    {401,  "Unauthorized"},
    {402,  "Payment Required"},
    {403,  "Forbidden"},
    {404,  "Not Found"},
    {405,  "Method Not Allowed"},
    {406,  "Not Acceptable"},
    {407,  "Proxy Authentication Required"},
    {408,  "Request Timeout"},
    {409,  "Conflict"},
    {410,  "Gone"},
    {411,  "Length Required"},
    {412,  "Precondition Failed"},
    {413,  "Payload Too Large"},
    {414,  "URI Too Long"},
    {415,  "Unsupported Media Type"},
    {416,  "Range Not Satisfiable"},
    {417,  "Expectation Failed"},
    {418,  "I'm a teapot"},
    {421,  "Misdirected Request"},
    {422,  "Unprocessable Entity"},
    {423,  "Locked"},
    {424,  "Failed Dependency"},
    {425,  "Too Early"},
    {426,  "Upgrade Required"},
    {428,  "Precondition Required"},
    {429,  "Too Many Requests"},
    {431,  "Request Header Fields Too Large"},
    {451,  "Unavailable For Legal Reasons"},
    {501,  "Not Implemented"},
    {502,  "Bad Gateway"},
    {503,  "Service Unavailable"},
    {504,  "Gateway Timeout"},
    {505,  "HTTP Version Not Supported"},
    {506,  "Variant Also Negotiates"},
    {507,  "Insufficient Storage"},
    {508,  "Loop Detected"},
    {510,  "Not Extended"},
    {511,  "Network Authentication Required"}
};

inline const std::unordered_map<std::string, std::string> _mime_msg = {
    {".aac",        "audio/aac"},
    {".abw",        "application/x-abiword"},
    {".arc",        "application/x-freearc"},
    {".avi",        "video/x-msvideo"},
    {".azw",        "application/vnd.amazon.ebook"},
    {".bin",        "application/octet-stream"},
    {".bmp",        "image/bmp"},
    {".bz",         "application/x-bzip"},
    {".bz2",        "application/x-bzip2"},
    {".csh",        "application/x-csh"},
    {".css",        "text/css"},
    {".csv",        "text/csv"},
    {".doc",        "application/msword"},
    {".docx",       "application/vnd.openxmlformats-officedocument.wordprocessingml.document"},
    {".eot",        "application/vnd.ms-fontobject"},
    {".epub",       "application/epub+zip"},
    {".gif",        "image/gif"},
    {".htm",        "text/html"},
    {".html",       "text/html"},
    {".ico",        "image/vnd.microsoft.icon"},
    {".ics",        "text/calendar"},
    {".jar",        "application/java-archive"},
    {".jpeg",       "image/jpeg"},
    {".jpg",        "image/jpeg"},
    {".js",         "text/javascript"},
    {".json",       "application/json"},
    {".jsonld",     "application/ld+json"},
    {".mid",        "audio/midi"},
    {".midi",       "audio/x-midi"},
    {".mjs",        "text/javascript"},
    {".mp3",        "audio/mpeg"},
    {".mpeg",       "video/mpeg"},
    {".mpkg",       "application/vnd.apple.installer+xml"},
    {".odp",        "application/vnd.oasis.opendocument.presentation"},
    {".ods",        "application/vnd.oasis.opendocument.spreadsheet"},
    {".odt",        "application/vnd.oasis.opendocument.text"},
    {".oga",        "audio/ogg"},
    {".ogv",        "video/ogg"},
    {".ogx",        "application/ogg"},
    {".otf",        "font/otf"},
    {".png",        "image/png"},
    {".pdf",        "application/pdf"},
    {".ppt",        "application/vnd.ms-powerpoint"},
    {".pptx",       "application/vnd.openxmlformats-officedocument.presentationml.presentation"},
    {".rar",        "application/x-rar-compressed"},
    {".rtf",        "application/rtf"},
    {".sh",         "application/x-sh"},
    {".svg",        "image/svg+xml"},
    {".swf",        "application/x-shockwave-flash"},
    {".tar",        "application/x-tar"},
    {".tif",        "image/tiff"},
    {".tiff",       "image/tiff"},
    {".ttf",        "font/ttf"},
    {".txt",        "text/plain"},
    {".vsd",        "application/vnd.visio"},
    {".wav",        "audio/wav"},
    {".weba",       "audio/webm"},
    {".webm",       "video/webm"},
    {".webp",       "image/webp"},
    {".woff",       "font/woff"},
    {".woff2",      "font/woff2"},
    {".xhtml",      "application/xhtml+xml"},
    {".xls",        "application/vnd.ms-excel"},
    {".xlsx",       "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet"},
    {".xml",        "application/xml"},
    {".xul",        "application/vnd.mozilla.xul+xml"},
    {".zip",        "application/zip"},
    {".3gp",        "video/3gpp"},
    {".3g2",        "video/3gpp2"},
    {".7z",         "application/x-7z-compressed"}
};

class Util 
{
    public:
        //HTTP 头部字段名大小写不敏感
        static bool IEquals(const std::string &a, const std::string &b)
        {
            if (a.size() != b.size())
                return false;
            for (size_t i = 0; i < a.size(); ++i)
            {
                if (std::tolower(static_cast<unsigned char>(a[i])) !=
                    std::tolower(static_cast<unsigned char>(b[i])))
                    return false;
            }
            return true;
        }
        static std::string Trim(const std::string &s)
        {
            size_t begin = s.find_first_not_of(" \t");
            if (begin == std::string::npos)
                return "";
            size_t end = s.find_last_not_of(" \t");
            return s.substr(begin, end - begin + 1);
        }
        //这是一个字符串split分割函数，表示从src中按照seq字符逐个分割放入array
        static size_t Split(const std::string &src, const std::string &sep, std::vector<std::string> *arry) 
        {
            if (sep.empty())
                return arry->size();
            size_t offset = 0;//定义一个偏移量
            while(offset < src.size()) 
            {
                //从偏移量位置开始找seq找到了就以此为从offset位置截取到pos位置，没找到说明没有了把整个源字符串放回array
                size_t pos = src.find(sep, offset);
                if (pos == std::string::npos) 
                {
                    arry->push_back(src.substr(offset));
                    return arry->size();
                }
                if (pos == offset) //这种情况譬如1234\n\n123\n连续的分割符要跳过
                {
                    offset = pos + sep.size();
                    continue;
                }
                arry->push_back(src.substr(offset, pos - offset));
                offset = pos + sep.size();//更新offset的值
            }
            return arry->size();
        }
        
        //这是一个文件读取函数，从指定文件之中读入文件到buf缓冲区
        static bool ReadFile(const std::string &filename, std::string *buf) 
        {
            std::ifstream ifs(filename, std::ios::binary);//定义一个文件输入流，模式是直接二进制不处理
            if (ifs.is_open() == false) 
            {
                printf("OPEN %s FILE FAILED!!", filename.c_str());
                return false;
            }
            //seekg，参数1代表偏移参数2多少位置
            ifs.seekg(0, ifs.end);//把读指针移动到文件末尾
            std::streampos endPos = ifs.tellg();  //返回当前指针位置，也就是文件大小
            if (endPos < 0)
            {
                printf("GET %s FILE SIZE FAILED!!", filename.c_str());
                ifs.close();
                return false;
            }
            size_t fsize = static_cast<size_t>(endPos);
            ifs.seekg(0, ifs.beg);//把读指针移动到文件起始位置
            buf->resize(fsize);//扩容buffer 
            if (fsize > 0)
                ifs.read(&(*buf)[0], fsize);//将fsize大小的数据读到buffer中
            if (ifs.good() == false) //判断流状态是否正常
            {
                printf("READ %s FILE FAILED!!", filename.c_str());
                ifs.close();
                return false;
            }
            ifs.close();//关闭流
            return true;
        }
        //这个函数的含义是向指定文件写入buffer的数据
        static bool WriteFile(const std::string &filename, const std::string &buf) 
        {
            std::ofstream ofs(filename, std::ios::binary | std::ios::trunc);//打开一个文件如果文件存在则对其进行截断操作std::ios::trunc
            if (ofs.is_open() == false) 
            {
                printf("OPEN %s FILE FAILED!!", filename.c_str());
                return false;
            }
            ofs.write(buf.c_str(), buf.size());
            if (ofs.good() == false) {
                ERR_LOG("WRITE %s FILE FAILED!", filename.c_str());
                ofs.close();    
                return false;
            }
            ofs.close();
            return true;
        }
        //按照URL编码对请求行中的uri进行编码,第二个参数表示是否对空格进行处理
        static std::string UrlEncode(const std::string url, bool convert_space_to_plus) 
        {
            //char是一个8位字符，它可以表示ascii码对应的所有特殊字符，底层内存仍然保存的是数据
            //存汉字在utf-8的情况下需要三个字节，但是计算机底层寸的仍是数字，并且c++本身是无法打印的，cout直接输出那是显示器做的utf-8解析,编译器出现的汉字会转为utf-8编码
            //譬如string s = "中";本质上把中变成了一串24位的数字十六进制
            //而URL编码则是把规定要进行的字符把其变成16进制表示。内存其实并没变只是把它应该打印出来的值改为16进制
            std::string res;
            for (auto &c : url) 
            {
                unsigned char uc = static_cast<unsigned char>(c);
                if (c == '.' || c == '-' || c == '_' || c == '~' || std::isalnum(uc)) //标准不做处理
                {
                    res += c;
                    continue;
                }
                if (c == ' ' && convert_space_to_plus == true) 
                {
                    res += '+';//+ 表示的是空格
                    continue;
                }
                
                char tmp[4] = {0};
                snprintf(tmp, 4, "%%%02X", uc);//%%表示输出一个真正的%号，%02X表示输出两位大写16进制数,
                res += tmp;
            }
            return res;
        }
        //把十六进制字符数转化为原字符，非法字符返回 -1
        static int HEXTOI(char c) 
        {
            if (c >= '0' && c <= '9') 
                return c - '0';
            if (c >= 'a' && c <= 'f') 
                return c - 'a' + 10;
            if (c >= 'A' && c <= 'F') 
                return c - 'A' + 10;
            return -1; 
        }
        //进行URL的解码
        static std::string UrlDecode(const std::string url, bool convert_plus_to_space) 
        {
            std::string res;
            for (size_t i = 0; i < url.size(); i++) 
            {
                if (url[i] == '+' && convert_plus_to_space == true) 
                {
                    res += ' ';//变回空格
                    continue;
                }
                if (url[i] == '%' && (i + 2) < url.size())//当前位位%并且+2小于总长说明这里进行了url编码需要解码
                {
                    //譬如是一个E9
                    int v1 = HEXTOI(url[i + 1]);//一个字符E,不调用接口的话底层寸的是0X45（十进制69），但是这个表示的是原字符的高4位的值，要先变回原本的val
                    int v2 = HEXTOI(url[i + 2]);//一个字符9
                    if (v1 >= 0 && v2 >= 0)
                    {
                        char v = static_cast<char>(v1 * 16 + v2);//计算原来的ascii码值
                        res += v;
                        i += 2;
                        continue;
                    }
                }
                res += url[i];
            }
            return res;
        }
        //根据状态码返回对应其描述的字符串
        static std::string StatuDesc(int statu)
        {
            
            auto it = _statu_msg.find(statu);
            if (it != _statu_msg.end()) 
            {
                return it->second;
            }
            return "Unknow";
        }
        //根据文件名获取这个文件的含义 
        static std::string ExtMime(const std::string &filename) 
        {
            
            // a.b.txt  先获取文件扩展名
            size_t pos = filename.find_last_of('.');
            if (pos == std::string::npos) 
            {
                return "application/octet-stream";
            }

            std::string ext = filename.substr(pos);
            auto it = _mime_msg.find(ext);
            if (it == _mime_msg.end()) 
            {
                return "application/octet-stream";
            }
            return it->second;
        }
        //判断一个文件是不是目录 
        static bool IsDirectory(const std::string &filename) 
        {
            struct stat st;
            int ret = stat(filename.c_str(), &st);
            if (ret < 0) 
            {
                return false;
            }
            return S_ISDIR(st.st_mode);
        }
       //判断一个文件是不是普通文件
        static bool IsRegular(const std::string &filename) 
        {
            struct stat st;
            int ret = stat(filename.c_str(), &st);
            if (ret < 0) 
            {
                return false;
            }
            return S_ISREG(st.st_mode);
        }
        //判断写入路径是否是非法的
        static bool ValidPath(const std::string &path) 
        {
            //带 NUL 的路径会在 stat/open 时被截断，直接拒绝
            if (path.find('\0') != std::string::npos)
                return false;
            std::vector<std::string> subdir;
            Split(path, "/", &subdir);
            int level = 0;
            for (auto &dir : subdir) 
            {
                if (dir == ".") //"." 不改变目录层级
                {
                    continue;
                }
                if (dir == "..") 
                {
                    level--; 
                    if (level < 0) return false;
                    continue;
                }
                level++;
            }
            return true;
        }
};
class HttpRequest
{
public:
    std::string _method;
    std::string _path;
    std::string _body;
    std::string _version;
    std::smatch _matches;
    std::unordered_map<std::string, std::string> _headers;
    std::unordered_map<std::string, std::string> _params;
public:
    HttpRequest()
        :_version("HTTP/1.1")
    {

    }
    ~HttpRequest()
    {

    }
    //重置报文下报文内容
    void Reset()
    {
        _method.clear();
        _path.clear();
        _version = "HTTP/1.1";
        _body.clear();
        std::smatch match;
        _matches.swap(match);
        _headers.clear();
        _params.clear();
    }
    void SetHeader(const std::string &key, const std::string &val) 
    {
        _headers.insert(std::make_pair(key, val));
    }
    //插入一个头部字段 key: value
    void InserHeader(const std::string& key,const std::string& val) { _headers.insert(std::make_pair(key,val)); }
private:
    //HTTP 头部字段名大小写不敏感，先按原 key 查，再忽略大小写查
    bool FindHeader(const std::string &key, std::string *val) const
    {
        auto it = _headers.find(key);
        if (it != _headers.end())
        {
            if (val) *val = it->second;
            return true;
        }
        for (const auto &item : _headers)
        {
            if (Util::IEquals(item.first, key))
            {
                if (val) *val = item.second;
                return true;
            }
        }
        return false;
    }
public:
    //判断是否还有某个头部字段
    bool HasHeader(const std::string& key) const
    {
        return FindHeader(key, nullptr);
    }
    //根据key获得头部字段
    std::string GetHeader(const std::string &key) const 
    {
        std::string val;
        FindHeader(key, &val);
        return val;
    }
    //插入查询字符串
    void SetParam(const std::string &key, const std::string &val) 
    {
        _params.insert(std::make_pair(key, val));
    }
    //判断是否有某个查询字符串
    bool HasParam(const std::string &key) const 
    {
        auto it = _params.find(key);
        if (it == _params.end()) 
        {
            return false;
        }
            return true;
    }
    //根据key获得某个查询字符串
    std::string GetParam(const std::string &key) const 
    {
        auto it = _params.find(key);
        if (it == _params.end()) 
        {
            return "";
        }
            return it->second;
    }
    //获取正文长度，非法或超长时返回 false，避免 std::stol 抛异常导致进程 abort
    bool TryContentLength(size_t *len) const
    {
        *len = 0;
        std::string clen;
        if (FindHeader("Content-Length", &clen) == false)
        {
            return true;
        }
        if (clen.empty())
        {
            return false;
        }
        size_t value = 0;
        for (char c : clen)
        {
            if (c < '0' || c > '9')
            {
                return false;
            }
            size_t digit = c - '0';
            if (value > (std::numeric_limits<size_t>::max() - digit) / 10)
            {
                return false;
            }
            value = value * 10 + digit;
        }
        *len = value;
        return true;
    }
    //获取正文长度
    size_t ContentLength() const 
    {
        size_t len = 0;
        TryContentLength(&len);
        return len;
    }
private:
    //Connection 头可能是逗号分隔的 token 列表，例如 "keep-alive, Upgrade"
    bool ConnectionHasToken(const std::string &token) const
    {
        std::vector<std::string> tokens;
        Util::Split(GetHeader("Connection"), ",", &tokens);
        for (const auto &item : tokens)
        {
            if (Util::IEquals(Util::Trim(item), token))
                return true;
        }
        return false;
    }
public:
    //判断是否是短链接
    bool Close() const 
    {
        bool closeFlag = ConnectionHasToken("close");
        bool keepAliveFlag = ConnectionHasToken("keep-alive");
        //HTTP/1.0 默认短连接，除非显式 keep-alive；HTTP/1.1 默认长连接
        if (Util::IEquals(_version, "HTTP/1.0"))
            return closeFlag || keepAliveFlag == false;
        return closeFlag;
    }
};

class HttpResponse 
{
public:
    int _statu;
    bool _redirect_flag;
    std::string _body;
    std::string _redirect_url;
    std::unordered_map<std::string, std::string> _headers;
public:
    HttpResponse():_statu(200), _redirect_flag(false) {}
    HttpResponse(int statu):_statu(statu), _redirect_flag(false) {} 
    void ReSet() 
    {
        _statu = 200;
        _redirect_flag = false;
        _body.clear();
        _redirect_url.clear();
        _headers.clear();
    }
    //设置头部字段，同名（忽略大小写）的旧值会被覆盖
    void SetHeader(const std::string &key, const std::string &val) 
    {
        for (auto it = _headers.begin(); it != _headers.end(); )
        {
            if (Util::IEquals(it->first, key))
                it = _headers.erase(it);
            else
                ++it;
        }
        _headers.insert(std::make_pair(key, val));
    }
    void RemoveHeader(const std::string &key)
    {
        for (auto it = _headers.begin(); it != _headers.end(); )
        {
            if (Util::IEquals(it->first, key))
                it = _headers.erase(it);
            else
                ++it;
        }
    }
private:
    bool FindHeader(const std::string &key, std::string *val) const
    {
        auto it = _headers.find(key);
        if (it != _headers.end())
        {
            if (val) *val = it->second;
            return true;
        }
        for (const auto &item : _headers)
        {
            if (Util::IEquals(item.first, key))
            {
                if (val) *val = item.second;
                return true;
            }
        }
        return false;
    }
public:
    //判断是否存在指定头部字段
    bool HasHeader(const std::string &key) const
    {
        return FindHeader(key, nullptr);
    }
    //获取指定头部字段的值
    std::string GetHeader(const std::string &key) const
    {
        std::string val;
        FindHeader(key, &val);
        return val;
    }
    //设置body和解析类型
    void SetContent(const std::string &body,  const std::string &type = "text/html") 
    {
        _body = body;
        SetHeader("Content-Type", type);
    }
    //设置相对资源的具体位置
    void SetRedirect(const std::string &url, int statu = 302) 
    {
        _statu = statu;
        _redirect_flag = true;
        _redirect_url = url;
    }
    //判断是否是短链接
    bool Close() const
    {
        return Util::IEquals(GetHeader("Connection"), "keep-alive") == false;
    }
};
#define MAX_LINE 8192
#define MAX_HEADERS 100
enum HttpRecvStatus
{
    RECV_HTTP_ERROR,
    RECV_HTTP_LINE,
    RECV_HTTP_HEAD,
    RECV_HTTP_BODY,
    RECV_HTTP_OVER
};

class HttpContext
{
private:
    int _rspStatus;
    HttpRecvStatus _recvStatus;
    HttpRequest _request;
    size_t _headerLineCount;
    bool _continueSent;
private:
    bool ParseHttpLine(const std::string& line)
    {

        //1.根据正则表达式解析整个报文
        static const std::regex e("(GET|HEAD|POST|PUT|DELETE) ([^?]*)(?:\\?(.*))? (HTTP/1\\.[01])(?:\n|\r\n)?", std::regex::icase);
        std::smatch match;
        bool ret = std::regex_match (line,match,e);//把line进行解析如果匹配上了就将匹配的放入match.有一个不匹配返回false
        if(ret == false)
        {
            //设置接收到的报文状态
            _recvStatus = HttpRecvStatus::RECV_HTTP_ERROR;
            _rspStatus = 400;
            return false;
        }
        //0 : GET /bitejiuyeke/login?user=xiaoming&pass=123123 HTTP/1.1
        //1 : GET
        //2 : /bi/login
        //3 : user=xiaoming&pass=123123
        //4 : HTTP/1.1
        //这里代表接收到完整http请求报文
        //2.根据报文填充request；
        //填充方法
        _request._method = match[1];
        std::transform(_request._method.begin(), _request._method.end(), _request._method.begin(), ::toupper);//将方法转化为大写
        //填充资源路径
        _request._path = Util::UrlDecode(match[2],false);
        //origin-form 的请求目标必须以 '/' 开头，否则 _basedir + path 可能拼到根目录之外
        if(_request._path.empty() || _request._path[0] != '/' ||
           _request._path.find('\0') != std::string::npos)
        {
            _recvStatus = HttpRecvStatus::RECV_HTTP_ERROR;
            _rspStatus = 400;
            return false;
        }
        //填充查询字符串
        std::vector<std::string> paramsArray;
        Util::Split(match[3],"&", &paramsArray);
        for(auto& s : paramsArray)
        {
            size_t pos = s.find("=");
            if(pos == std::string::npos)
            {
                _recvStatus = HttpRecvStatus::RECV_HTTP_ERROR;
                _rspStatus = 400;//bad request
                return false;
            }

            std::string key = Util::UrlDecode(s.substr(0,pos),true);
            std::string val = Util::UrlDecode(s.substr(pos+1), true);
            _request.SetParam(key,val);
        }
        //填充Http版本，统一转成大写，如把 http/1.1 规范成 HTTP/1.1
        _request._version = match[4];
        std::transform(_request._version.begin(), _request._version.end(),
                       _request._version.begin(), ::toupper);
        return true; 
    }
    //对http请求行进行解析并作差错处理
    bool RecvHttpLine(Buffer* buffer)
    {   
        if(_recvStatus != HttpRecvStatus::RECV_HTTP_LINE) return false;
        std::string line = buffer->GetLineAndPop();
        if(line.size() == 0)
        {
            if(buffer->GetReadableSize() > MAX_LINE)
            {
                _rspStatus = 414;//uri too big
                _recvStatus = HttpRecvStatus::RECV_HTTP_ERROR;
                return false;
            }
            return true;
        }
        if(line.size() > MAX_LINE)
        {
            _rspStatus = 414;//uri too big
            _recvStatus = HttpRecvStatus::RECV_HTTP_ERROR;
            return false;
        }
        //接收到了请求行
        bool ret = ParseHttpLine(line);
        if(ret == true)
        {
            _recvStatus = RECV_HTTP_HEAD;
            return true;
        }
        return false;
    }
    bool RecvHttpHead(Buffer* buffer)
    {
        if(_recvStatus != HttpRecvStatus::RECV_HTTP_HEAD) return false;
        std::string line;
        while(1)
        {
            line = buffer->GetLineAndPop();
            if(line.size() == 0)
            {
                if(buffer->GetReadableSize() > MAX_LINE)
                {
                    _rspStatus = 414;//uri too big
                    _recvStatus = HttpRecvStatus::RECV_HTTP_ERROR;
                    return false;
                }
                return true;
            }
            if(line.size() > MAX_LINE)
            {
                _rspStatus = 414;//uri too big
                _recvStatus = HttpRecvStatus::RECV_HTTP_ERROR;
                return false;
            }

            if(line=="\n" || line == "\r\n")//遇到空行代表头部解析完毕了
                break;
            if(++_headerLineCount > MAX_HEADERS)
            {
                _rspStatus = 431;//request header fields too large
                _recvStatus = HttpRecvStatus::RECV_HTTP_ERROR;
                return false;
            }
            int ret = ParseHttpHead(line);
            if(ret == false)
                return false;
        }
        _recvStatus = HttpRecvStatus::RECV_HTTP_BODY;
        return true; 
    }
    //解析头部报文
    bool ParseHttpHead(std::string &line)
    {
        if(line.empty() == false && line.back() == '\n') line.pop_back();//顺序特别重要，http报文后面是标准的\r\n,这么写是有不标准的情况只有\n
        if(line.empty() == false && line.back() == '\r') line.pop_back();
        size_t pos = line.find(':');
        if(pos == std::string::npos || pos == 0)
        {
            _rspStatus = 400;
            _recvStatus = HttpRecvStatus::RECV_HTTP_ERROR;
            return false;
        }
        std::string key = line.substr(0,pos);
        if(key.find_first_of(" \t") != std::string::npos)
        {
            _rspStatus = 400;
            _recvStatus = HttpRecvStatus::RECV_HTTP_ERROR;
            return false;
        }
        //冒号后面的前导空白是可选的，值两端的空白去掉
        std::string val = Util::Trim(line.substr(pos + 1));
        //不支持的传输编码直接拒绝，避免把 chunked 正文当成下一个请求
        if (Util::IEquals(key, "Transfer-Encoding"))
        {
            _rspStatus = 501;
            _recvStatus = HttpRecvStatus::RECV_HTTP_ERROR;
            return false;
        }
        //重复的 Content-Length 具有请求走私风险，直接拒绝
        if (Util::IEquals(key, "Content-Length") && _request.HasHeader("Content-Length"))
        {
            _rspStatus = 400;
            _recvStatus = HttpRecvStatus::RECV_HTTP_ERROR;
            return false;
        }
        _request.SetHeader(key,val);
        return true;
    }
    bool RecvHttpBody(Buffer *buf) 
    {
        //http content-length表示的是body的大小
        if(_recvStatus != HttpRecvStatus::RECV_HTTP_BODY) return false;
        size_t contentLen = 0;
        if(_request.TryContentLength(&contentLen) == false)
        {
            _rspStatus = 400;
            _recvStatus = RECV_HTTP_ERROR;
            return false;
        }
        if(contentLen == 0)
        {
            _recvStatus = RECV_HTTP_OVER;
            return true;
        }
        if(_request._body.size() >= contentLen)
        {
            _recvStatus = RECV_HTTP_OVER;
            return true;
        }
        size_t len = contentLen - _request._body.size();
        if(buf->GetReadableSize() >= len)
        {
            _request._body.append(buf->GetReadPos(),len);
            buf->UpdateReadPos(len);
            _recvStatus = RECV_HTTP_OVER;
            return true;
        }
        size_t n = buf->GetReadableSize();
        _request._body.append(buf->GetReadPos(),n);
        buf->UpdateReadPos(n);
        return true;   
    }      
public:
    HttpContext():_rspStatus(200), _recvStatus(RECV_HTTP_LINE), _headerLineCount(0), _continueSent(false) {}
    ~HttpContext() { }
    int RespStatu() { return _rspStatus; }
    HttpRecvStatus RecvStatu() { return _recvStatus; }
    HttpRequest &Request() { return _request; }
    bool ContinueSent() { return _continueSent; }
    void MarkContinueSent() { _continueSent = true; }
    void ReSet() 
    {
        _rspStatus = 200;
        _recvStatus = RECV_HTTP_LINE;
        _headerLineCount = 0;
        _continueSent = false;
        _request.Reset();
    }
    void RecvHttpRequest(Buffer *buf) 
    {
        //不同的状态，做不同的事情，但是这里不要break， 因为处理完请求行后，应该立即处理头部，而不是退出等新数据
        switch(_recvStatus) 
        {
            case RECV_HTTP_LINE:
                RecvHttpLine(buf);
                [[fallthrough]];
            case RECV_HTTP_HEAD:
                RecvHttpHead(buf);
                [[fallthrough]];
            case RECV_HTTP_BODY:
                RecvHttpBody(buf);
                break;
            case RECV_HTTP_ERROR:
            case RECV_HTTP_OVER:
                break;
        }
            return;
    }

};

class HttpServer
{
private:
    using Handler = std::function<void(const HttpRequest &, HttpResponse *)>;
    using Handlers = std::vector<std::pair<std::regex, Handler>>;//回调函数处理集
    Handlers _get_route;
    Handlers _post_route;
    Handlers _put_route;
    Handlers _delete_route;
    std::string _basedir; //静态资源根目录
    TcpServer _server;
private:
    //请求错误时返回的页面
    void ErrorHandler(const HttpRequest &req, HttpResponse *rsp) 
    {
            std::string body;
            body += "<html>";
            body += "<head>";
            body += "<meta http-equiv='Content-Type' content='text/html;charset=utf-8'>";
            body += "</head>";
            body += "<body>";
            body += "<h1>";
            body += std::to_string(rsp->_statu);
            body += " ";
            body += Util::StatuDesc(rsp->_statu);
            body += "</h1>";
            body += "</body>";
            body += "</html>";
            rsp->SetContent(body, "text/html");//设置返回正文
    }
    //对conn连接返回一个处理报文。forceClose 用于错误响应：即使请求写了 keep-alive，服务端也要关闭
    void WriteReponse(const PtrConnection &conn, const HttpRequest &req, HttpResponse &rsp, bool forceClose = false)
    {
        bool close = forceClose || req.Close();
        rsp.SetHeader("Connection", close ? "close" : "keep-alive");

        //1xx/204/304 不能带正文，也不应该带 Content-Length
        bool noBodyStatus = (rsp._statu == 204 || rsp._statu == 304 ||
                             (rsp._statu >= 100 && rsp._statu < 200));
        if (noBodyStatus)
        {
            rsp.RemoveHeader("Content-Length");
        }
        else
        {
            //即使 body 为空也要发 Content-Length: 0，否则 keep-alive 下客户端无法判断响应结束
            rsp.SetHeader("Content-Length", std::to_string(rsp._body.size()));
        }
        //显示告诉客户端应该怎么处理body
        if (rsp._body.empty() == false && rsp.HasHeader("Content-Type") == false) { rsp.SetHeader("Content-Type", "application/octet-stream"); }
        //设置重定向uri具体位置
        if (rsp._redirect_flag == true) { rsp.SetHeader("Location", rsp._redirect_url); }
        //组织response报文
        std::stringstream rsp_str;
        rsp_str << req._version << " " << std::to_string(rsp._statu) << " " << Util::StatuDesc(rsp._statu) << "\r\n";
        for (auto &head : rsp._headers) 
        {
            rsp_str << head.first << ": " << head.second << "\r\n";
        }
        rsp_str << "\r\n";
        //HEAD 只返回头部；1xx/204/304 也不带 body
        if(req._method != "HEAD" && noBodyStatus == false)
            rsp_str << rsp._body;

        std::string response = rsp_str.str();
        conn->Send(response.c_str(), response.size());//发送给muduo的outbuffer

    }
    void OnConnect(const PtrConnection& conn)
    {
        conn->SetContext(HttpContext());
        DBG_LOG("NEW CONNECTION %p", static_cast<void *>(conn.get()));
    }
    void FileHandler(const HttpRequest &req,HttpResponse* rsp)
    {
        std::string reqPath = _basedir + req._path;
        if(reqPath.back() == '/')
        {
            reqPath+="index.html";
        }
        if(Util::ReadFile(reqPath, &rsp->_body) == false)
        {
            rsp->_body.clear(); //读失败时可能已读到半截内容，不能把半截文件发出去
            rsp->_statu = 404;
            return;
        }
        //MIME 必须按实际文件路径取，否则 "/" 会取到 application/octet-stream
        rsp->SetHeader("Content-Type", Util::ExtMime(reqPath));
        return;

    }

    bool IsFileHandler(const HttpRequest &req)
    {
        if(_basedir.empty() == true)
            return false;
        if(req._method != "GET" && req._method != "HEAD")
            return false;
        if(Util::ValidPath(req._path) == false)
            return false;

        std::string path = _basedir + req._path;
        if(path.back() == '/')
            path.append("index.html");
        if(Util::IsRegular(path) == false)
            return false;
        return true;
    }
    void Dispatcher(HttpRequest& req,HttpResponse* rsp,Handlers& handles)
    {
        bool matched = false;
        for(auto& handle : handles)
        {
            const std::regex& e = handle.first;
            const Handler& func = handle.second;
            bool ret = std::regex_match (req._path,req._matches,e);//把req._path与e匹配的结果放到_mathces之中
            if(ret == false)  continue;
            matched = true;
            func(req,rsp);
        }
        if(matched == false)
            rsp->_statu = 404;
    }
    void Route(HttpRequest &req, HttpResponse *rsp)
    {
        //静态资源的请求
        if(IsFileHandler(req) == true)
        {
            return FileHandler(req,rsp);
        }
        if (req._method == "GET" || req._method == "HEAD") 
        {
            return Dispatcher(req, rsp, _get_route);
        }
        else if (req._method == "POST") 
        {
            return Dispatcher(req, rsp, _post_route);
        }
        else if (req._method == "PUT") 
        {
            return Dispatcher(req, rsp, _put_route);
        }
        else if (req._method == "DELETE") 
        {
            return Dispatcher(req, rsp, _delete_route);
        }
            rsp->_statu = 405;// Method Not Allowed
            return ;

    }

    void OnMessage(const PtrConnection& conn,Buffer* buf)
    {
        try
        {
            //必须使用连接里保存的上下文，不能拷贝：否则一次读不全的请求
            //（请求行/头部/正文被拆到多个TCP报文）下次会从初始状态重新解析。
            HttpContext& context = conn->GetContext()->GetData<HttpContext>();
            while(buf->GetReadableSize() > 0)
            {
                context.RecvHttpRequest(buf);//对buffer缓冲区的内容进行解析
                HttpRequest& request = context.Request();
                //判断接收数据正不正常
                if(context.RespStatu() >= 400) 
                {
                    HttpResponse response(context.RespStatu());//给回复报文设置错误码
                    ErrorHandler(request,&response);//往body中填入一个错误html页面
                    WriteReponse(conn, request, response, true);
                    // buf->Clear();
                    buf->UpdateReadPos(buf->GetReadableSize());//把读位置放到写位置,这样做到了clear
                    context.ReSet();
                    conn->Shutdown();//关闭连接
                    return;
                }
                //Expect: 100-continue：头部收完后先回 100，客户端才会发送正文
                if(context.RecvStatu() == RECV_HTTP_BODY)
                {
                    std::string expect = Util::Trim(request.GetHeader("Expect"));
                    if(expect.empty() == false &&
                       Util::IEquals(expect, "100-continue") == false)
                    {
                        HttpResponse response(417);
                        ErrorHandler(request,&response);
                        WriteReponse(conn, request, response, true);
                        buf->UpdateReadPos(buf->GetReadableSize());
                        context.ReSet();
                        conn->Shutdown();
                        return;
                    }
                    if(Util::IEquals(expect, "100-continue") && context.ContinueSent() == false)
                    {
                        std::string cont = request._version + " 100 Continue\r\n\r\n";
                        conn->Send(cont.c_str(), cont.size());
                        context.MarkContinueSent();
                    }
                }
                //请求还没收完整，保留上下文，等下一次读事件继续解析
                if(context.RecvStatu() != RECV_HTTP_OVER)
                    return;
                HttpResponse response(context.RespStatu());
                Route(request,&response);
                WriteReponse(conn, request, response);
                bool close = response.Close();
                context.ReSet();
                if(close == true)
                {
                    //响应已经声明 close，后面的流水线数据直接丢弃
                    buf->UpdateReadPos(buf->GetReadableSize());
                    conn->Shutdown();
                    return;
                }
            }
        }
        catch (const std::exception &e)
        {
            //业务回调或内存分配抛异常时不能让整个进程 terminate
            ERR_LOG("OnMessage exception: %s", e.what());
            buf->UpdateReadPos(buf->GetReadableSize());
            conn->Shutdown();
        }
        catch (...)
        {
            ERR_LOG("OnMessage unknown exception");
            buf->UpdateReadPos(buf->GetReadableSize());
            conn->Shutdown();
        }
        return;
    }
public:
    HttpServer(uint64_t capacity,uint16_t port,uint64_t timeout):_server(capacity,port,timeout)
    {
            _server.EnableInactiveRelease(timeout);
            _server.SetConnectedCallBack(std::bind(&HttpServer::OnConnect, this, std::placeholders::_1));
            _server.SetMessageCallBack(std::bind(&HttpServer::OnMessage, this, std::placeholders::_1, std::placeholders::_2));
    }
    void SetBaseDir(const std::string &path) 
    {
        if (Util::IsDirectory(path) == false)
        {
            ERR_LOG("BaseDir '%s' is not a directory", path.c_str());
            exit(1);
        }
        _basedir = path;
    }
        /*设置/添加，请求（请求的正则表达）与处理函数的映射关系*/
    void Get(const std::string &pattern, const Handler &handler) 
    {
        _get_route.push_back(std::make_pair(std::regex(pattern), handler));
    }
    void Post(const std::string &pattern, const Handler &handler) 
    {
        _post_route.push_back(std::make_pair(std::regex(pattern), handler));
    }
    void Put(const std::string &pattern, const Handler &handler) 
    {
        _put_route.push_back(std::make_pair(std::regex(pattern), handler));
    }
    void Delete(const std::string &pattern, const Handler &handler) 
    {
        _delete_route.push_back(std::make_pair(std::regex(pattern), handler));
    }
    void SetThreadCount(int count) 
    {
        _server.SetThreadPoolNums(count);
    }
    void Listen() 
    {
        _server.Start();
    }
};