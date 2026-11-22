/*
 * @Author: HejieY 2145381080@qq.com
 * @Date: 2026-10-13 19:32:26
 * @FilePath: /muduo-http-server/source/Server.hpp
 * @Description: 这个类是高性能服务器的缓冲区,核心思想是使用vector<char>来存储数据,双指针维护空间
 * @Questions:
 * 1.为什么类内设计时都是用地址而不用下标？：系统调用参数为地址，用地址方便与系统调用协调
 * 2.取地址返回的是指针,强转uint64_t就可以拿到具体的地址
 * 3.void* 加法是未定义问题,步长是不确定的
 * Copyright (c) 2026 by HeJieY, All Rights Reserved.
 */
#pragma once
#include <sys/time.h>
#include<sys/timerfd.h>
#include <sys/epoll.h>
#include <asm-generic/socket.h>
#include <cerrno>
#include <cstddef>
#include <cstdio>
#include <functional>
#include <string>
#include <ctime>
#include <cstring>
#include <algorithm>
#include <cstdint>
#include <sys/types.h>
#include <unordered_map>
#include <utility>
#include <vector>
#include <cstdint>
#include <cassert>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <thread>
#include <sys/eventfd.h>
#include <mutex>
#include<time.h>
#include<memory>
#include<condition_variable>

enum ExitCode
{
    EPOLLFD_CREATE_ERROR = 1,
    CREATE_SOCKET_ERROR,
    BIND_ERROR,
    LISTEN_SOCKET_CREATE_ERROR,
    EPOLL_CTL_ERROR,
    EPOLL_WAIT_ERROR,
    ACCEPT_ERROR,
    WRITE_ERROR,
    READ_ERROR,
    RECV_ERROR,
    TIMERFD_CREATE_ERROR,
    NOT_FIND_TIMERTASK,
};



#define BUFFER_SIZE 1024
#define INF 0
#define DBG 1
#define ERR 2
#define LOG_LEVEL INF

#define LOG(level, fmt, ...)                                           \
    do                                                                 \
    {                                                                  \
        if (level >= LOG_LEVEL)                                        \
        {                                                              \
            time_t t = time(NULL);                                     \
            struct tm *tm_info = localtime(&t);                        \
            char time_buf[9];                                          \
            strftime(time_buf, sizeof(time_buf), "%H:%M:%S", tm_info); \
            fprintf(stdout, "[%s] %s:%d " fmt "\n",                    \
                    time_buf, __FILE__, __LINE__, ##__VA_ARGS__);      \
        }                                                              \
    } while (0)

#define INF_LOG(fmt, ...) LOG(INF, fmt, ##__VA_ARGS__)
#define DBG_LOG(fmt, ...) LOG(DBG, fmt, ##__VA_ARGS__)
#define ERR_LOG(fmt, ...) LOG(ERR, fmt, ##__VA_ARGS__)


class Any
{
private:
    class Holder
    {
        public:
            virtual ~Holder(){}
            virtual const std::type_info& GetType() = 0;
            virtual Holder* Clone() = 0;
    };
    template<typename T>
    class PlaceHolder : public Holder
    {
        public:
            T _data;
        public:
            PlaceHolder(T data):_data(data){}
            ~PlaceHolder(){}
            const std::type_info& GetType() override
            { return typeid(_data); }
            Holder* Clone() override
            { return new PlaceHolder<T>(_data); }
    };
    Holder* _content;
public:
    // 这个写法比较新，可以多看看
    void Swap(Any& other)
    {
        Holder* tmp = this->_content;
        _content = other._content;
        other._content = tmp;
    }
    Any():_content(nullptr) {}

    template<typename T>
    Any(const T& val)
        :_content(new PlaceHolder<T>(val)) {}
    Any(const Any& other)
        :_content(other._content ? other._content->Clone() : nullptr) {}
    Any& operator=(const Any& other)
    {
        if(this!=&other)
        {
            Any tmp(other);
            Swap(tmp);
        }
       return *this;
    }
    // 为什么当赋值重载碰上模板函数不用<>了？
    // 函数模板当给予实参时，会自动退到类型，不用显示<>
    template<typename T>
    Any& operator=(const T& val)
    {
       Any(val).Swap(*this);
       return *this;
    }

    ~Any() { delete _content; }
    // 关于多态：
    // 子类指针访问父类时进行的是对象切片，不需要进行强转，这是安全的
    // 父类指针访问子类时:对于发生多态的函数不用强转，对于子类自己的函数则必须进行强转
    template<typename T>
    T GetData()
    {
        assert(typeid(T) == _content->GetType());
        return ((PlaceHolder<T>*)_content)->_data;
    }
};



class EventLoop;
class Buffer
{
public:
    Buffer() : _read_index(0), _write_index(0), _capacity(BUFFER_SIZE), _buffer(_capacity) {}
    ~Buffer() {}
    //包含了更新位置
    uint64_t Write(const void *src, uint64_t len)
    {
        if(len == 0)
            return 0;
        EnsureMemEnough(len);
        const char *data = (const char *)src;
        std::copy(data, data + len, GetWritePos());
        // 更新读写位置
        UpdateWritePos(len);
        return len;
    }
    //包含了更新位置，len表示的是要读多少数据
    uint64_t Read(void *dst, uint64_t len)
    {
        Peek(dst, len);
        UpdateReadPos(len);
        return len;
    }
    uint64_t Peek(void *dst, uint64_t len)
    {
        assert(len <= GetReadableSize());
        std::copy(GetReadPos(), GetReadPos() + len, (char *)dst);
        return len;
    }
    std::string PeekAsString(uint64_t len)
    {
        assert(len <= GetReadableSize());
        return std::string(GetReadPos(), len);
    }
    uint64_t WriteAsString(const std::string &src) { return Write(src.c_str(), src.size()); }
    uint64_t WriteAsBuffer(Buffer &src)
    {
        if(&src == this)
            return 0;
        return Write(src.GetReadPos(), src.GetReadableSize());
    }

    std::string ReadAsString(uint64_t len)
    {
        assert(len <= GetReadableSize());
        std::string ret(GetReadPos(), len);
        UpdateReadPos(len);
        return ret;
    }

    bool Clear()
    {
        _read_index = 0;
        _write_index = 0;
        return true;
    }
    uint64_t GetReadableSize() { return _write_index - _read_index; }
    // 这个是为了一行一行的提取HTTP协议内容
    std::string ParseByCRLF()
    {
        auto it = std::find(GetReadPos(), GetWritePos(), '\n');
        if (it != GetWritePos())
        {
            return std::string(GetReadPos(), it + 1); // 把\n也提取出来
        }
        else
        {
            return "";
        }
    }
    char *GetReadPos() { return GetBegin() + _read_index; }
    char *GetWritePos() { return GetBegin() + _write_index; }
    // 老师的项目中将char* 写成了void*，查询资料得知void*的加法是未定义的，g++编译器把其看成1，考虑到这些，我们用char*
    void UpdateWritePos(uint64_t len) { _write_index += len; }
    void UpdateReadPos(uint64_t len) { _read_index += len; }
    char *GetBegin() { return &(*_buffer.begin()); }
private:
    uint64_t GetWriteTailMem() { return _capacity - _write_index; }
    uint64_t GetTotalMem() { return GetWriteTailMem() + GetReadFrontMem(); }
    uint64_t GetReadFrontMem() { return _read_index; }
    bool EnsureMemEnough(uint64_t len)
    {
        if (len <= GetWriteTailMem())
        {
            return true;
        }
        // 进行空间的挪移
        if (GetTotalMem() >= len)
        {
            uint64_t readAbleLen = GetReadableSize();
            std::memmove(GetBegin(), GetReadPos(), readAbleLen);
            _read_index = 0;
            _write_index = readAbleLen;
            return true;
        }
        // 进行合理的扩容和空间的挪移
        else
        {
            uint64_t readAbleLen = GetReadableSize();
            _buffer.resize(len * 2 + _capacity, 0);
            _capacity = len * 2 + _capacity;
            std::memmove(GetBegin(), GetReadPos(), readAbleLen);
            _read_index = 0;
            _write_index = readAbleLen;
            return true;
        }
    }

private:
    uint64_t _read_index;
    uint64_t _write_index;
    uint64_t _capacity;
    std::vector<char> _buffer;
};

class Socket
{
private:
    int _socketfd;

private:
public:
    Socket() : _socketfd(-1) {}

    Socket(int socketfd) : _socketfd(socketfd) {}
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;
    ~Socket() { Close(); }

    int GetSocket()
    {
        return _socketfd;
    }

    int CreateSocket()
    {
        _socketfd = socket(AF_INET, SOCK_STREAM, 0);
        if (_socketfd < 0)
        {
            ERR_LOG("Socketfd create error!");
            exit(ExitCode::CREATE_SOCKET_ERROR);
        }
        return _socketfd;
    }
    // Return: <0 is error
    int Bind(const std::string &ip, uint16_t port)
    {
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        if (inet_pton(AF_INET, ip.c_str(), &addr.sin_addr) <= 0)
        {
            ERR_LOG("Inet_pton error");
            return -1;
        }
        addr.sin_port = htons(port);
        socklen_t len = sizeof(addr);
        int ret = bind(_socketfd, (struct sockaddr *)&addr, len);
        if(ret < 0)
        {
            perror("Bind error");
        }
        return ret;
    }

    void SetReuseAddress(int op)
    {
        setsockopt(_socketfd, SOL_SOCKET, SO_REUSEADDR, &op, sizeof(op));
    }

    bool CreateClientConnect(const std::string &ip, uint16_t port)
    {
        if (CreateSocket() < 0)
        {
            exit(ExitCode::CREATE_SOCKET_ERROR);
        }
        sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);
        addr.sin_port = htons(port);
        addr.sin_family = AF_INET;
        socklen_t len = sizeof(addr);
        int ret = connect(_socketfd, (struct sockaddr *)&addr, len);
        if (ret < 0)
        {
            ERR_LOG("Connect error!");
            return false;
        }
        return true;
    }

    bool CreateServer(const std::string &ip, uint16_t port)
    {
        if (CreateSocket() < 0)
        {
            exit(ExitCode::CREATE_SOCKET_ERROR);
        }
        // 地址复用必须在bind之前
        SetReuseAddress(1);
        if (Bind(ip, port) < 0)
        {
            ERR_LOG("bind  error");
            exit(ExitCode::BIND_ERROR);

        }
        if (InitAsListenFd() < 0)
        {
            exit(ExitCode::LISTEN_SOCKET_CREATE_ERROR);
        }
        SetNonBlock();
        return true;
    }

    int ConnectToServer(const struct sockaddr *addr, socklen_t len)
    {
        int socketfd = connect(_socketfd, addr, len);
        if (socketfd < 0)
        {
            ERR_LOG("Connect error!");
        }
        return socketfd;
    }

    int InitAsListenFd()
    {
        return listen(_socketfd, 1024);
    }


    int GetNewConnect()
    {
        struct sockaddr addr;
        socklen_t len = sizeof(addr);
        memset(&addr, 0, len);
        int ret = accept(_socketfd, &addr, &len);
        if (ret < 0)
        {
            if (errno == EAGAIN || errno == EINTR)
            {
                return -1;
            }
            else
            {
                ERR_LOG("Accept error!");
                exit(ExitCode::ACCEPT_ERROR);
            }
        }
        else
        {
            return ret;
        }
    }

    bool Close()
    {
        if (_socketfd != -1)
        {
            close(_socketfd);
            _socketfd = -1;
        }
        return true;
    }

    ssize_t Send(const char *buffer, size_t len, int flag = 0)
    {
        ssize_t n = send(_socketfd, buffer, len, flag | MSG_NOSIGNAL);
        if (n < 0)
        {
            if (errno == EAGAIN || errno == EINTR)
            {
                return 0;
            }
            else
            {
                ERR_LOG("Send error");
                return -1;
            }
        }
        else
        {
            return n;
        }
    }

    ssize_t SendAsNon(const char *buffer, size_t len)
    {
        return Send(buffer, len, MSG_DONTWAIT);
    }

    ssize_t Recv(char *buffer, int n, int flag = 0)
    {
        ssize_t sz = recv(_socketfd, buffer, n, flag);
        // recv 返回 0 表示对端正常关闭，必须和 EAGAIN 区分开
        if (sz == 0)
            return -1;
        if (sz < 0)
        {
            if (errno == EAGAIN || errno == EINTR)
                return 0;
            return -1;
        }
        return sz;
    }

    ssize_t RecvAsNon(char *buffer, ssize_t n)
    {
        return Recv(buffer, n, MSG_DONTWAIT);
    }

    int FdCntl(int op, int addFlag)
    {
        int flag = fcntl(_socketfd, F_GETFL, 0);
        return fcntl(_socketfd, op, flag | addFlag);
    }

    bool SetNonBlock()
    {
        FdCntl(F_SETFL, O_NONBLOCK);
        return true;
    }

    int AcceptNewLink()
    {
        int newFd = accept(_socketfd, nullptr, nullptr);
        return newFd;
    }
};
class EventLoop;

class Channel
{
private:

    int _socketfd;
    EventLoop *_ptr;
    uint32_t _events;  // 表示的是某套接字要管理的事件
    uint32_t _revents; // 表示的是某套接字哪些事件被触发了
    using EventCallback = std::function<void()>;
    EventCallback _read_callback;
    EventCallback _write_callback;
    EventCallback _error_callback;
    EventCallback _close_callback;
    EventCallback _event_callback;

public:

    Channel(int socketfd, EventLoop *ptr) : _socketfd(socketfd), _ptr(ptr)
    {
        _events = 0;
        _revents = 0;
    }
    Channel():_socketfd(-1),_ptr(nullptr),_events(0),_revents(0) {}
    ~Channel() {}
    void SetFd(int fd) { _socketfd = fd; }
    int GetFd() { return _socketfd; }
    uint32_t GetEvent() { return _events; }
    void Update();
    void Remove();
    bool ReadAble()
    {
        if(_events & EPOLLIN)
            return true;
        return false;
    }
    bool WriteAble()
    {
        if(_events & EPOLLOUT)
            return true;
        return false;
    }
    //设置回调函数
    void SetEventCallback(EventCallback eventCallback) { _event_callback = eventCallback; }
    void SetWriteCallback(EventCallback writeCallback) { _write_callback = writeCallback; }
    void SetReadCallback(EventCallback readCallback)   { _read_callback = readCallback;   }
    void SetCloseCallback(EventCallback closeCallback) { _close_callback = closeCallback; }
    void SetErrorCallback(EventCallback errorCallback) { _error_callback = errorCallback; }
    void EnableRead()
    {
        _events |= EPOLLIN;
        Update();
    }
    void EnableWrite()
    {
        _events |= EPOLLOUT;
        Update();
    }
    void DisableRead()
    {
        _events &= ~EPOLLIN;
        Update();
    }
    void DisableWrite()
    {
        _events &= ~EPOLLOUT;
        Update();
    }
    void DisableAll()
    {
        _events = 0;
        Update();
    }
    //移除某事件
    void RemoveEvent(uint32_t event)
    {
        _events &= ~event;
        Update();
    }
    void SetRevents(uint32_t revents)
    {
        _revents = revents;
    }
    void CloseFd()
    {
        //如果一次事件里 HandleRead 已经关闭了连接，后面 EPOLLHUP 又调用 HandleClose，就会第二次CloseFd

        if (_socketfd < 0) return;
        Remove();
        close(_socketfd);
        INF_LOG("Close  fd:%d", _socketfd);
        _socketfd = -1;
    }

    void HandleEvent()
    {
        // 关于EPOLLHUP和EPOLLREDHUP
        // EPOLLHUP：指的是对方fd已不可使用，因为各种原因未收到FIN便断开，需要将该链接close
        // EPOLLRDHUP：指收到对方FIN，我方仍可发送数据，处理完业务后close
        if ((_revents & EPOLLIN) || (_revents & EPOLLRDHUP) || (_revents & EPOLLPRI))
        {
            if (_read_callback) _read_callback();
        }
        if (_revents & EPOLLOUT)
        {
            if (_write_callback) _write_callback();
        }
        else if (_revents & EPOLLERR)
        {
            if (_error_callback) _error_callback();
        }
        else if (_revents & EPOLLHUP)
        {
            if (_close_callback) _close_callback();
        }
        if(_event_callback) _event_callback();
    }
};

#define MAX_EVENTS 1024
class Poller
{
private:
    int _epollfd;
    std::unordered_map<int, Channel *> _map; // 理论上遍历所有Channel也可以但是效率低
    struct epoll_event _events[MAX_EVENTS];
    int Update(Channel *channel, int op)
    {
        struct epoll_event event;
        event.data.fd = channel->GetFd();
        event.events = channel->GetEvent();

        INF_LOG("Add a event");
        return epoll_ctl(_epollfd, op, channel->GetFd(), &event);
    }

public:
    Poller()
    {
        memset(_events, 0, sizeof(_events));
        _epollfd = epoll_create1(0);
        if (_epollfd < 0)
        {
            ERR_LOG("Epollfd create fail!");
            exit(ExitCode::EPOLLFD_CREATE_ERROR);
        }
        // std::cout << "Epoll init" << std::endl;
    }
    int UpdateEvent(Channel *channel)
    {
        bool existed = HaveChannel(channel);
        int ret;
        if (existed == false)
        {
            _map[channel->GetFd()] = channel;
            ret = Update(channel, EPOLL_CTL_ADD);
            if (ret < 0)
                _map.erase(channel->GetFd());
        }
        else
        {
            ret = Update(channel, EPOLL_CTL_MOD);
        }
        if (ret < 0)
        {
            ERR_LOG("Epoll control error!");
            perror("");
        }
        return ret;
    }
    bool HaveChannel(Channel *channel)
    {
        auto it = _map.find(channel->GetFd());
        if (it != _map.end())
            return true;
        return false;
    }
    void RemoveListen(Channel *channel)
    {
        // if(HaveChannel(channel) == false)
        //     _map[channel->GetFd()] = channel;
        int ret = Update(channel, EPOLL_CTL_DEL);
        if (ret < 0 && errno != ENOENT && errno != EBADF)
        {
            ERR_LOG("Epoll control error!");
            exit(ExitCode::EPOLL_CTL_ERROR);
        }
        RemoveMapDate(channel->GetFd());
    }
    int Poll(std::vector<Channel *> *active)
    {
        int ret = epoll_wait(_epollfd, _events, MAX_EVENTS, -1);
        if (ret == 0)
        {
            return 0;
        }
        else if (ret < 0)
        {
            if (errno == EINTR)
            {
                return -1;
            }
            else
            {
                exit(ExitCode::EPOLL_WAIT_ERROR);
            }
        }
        else
        {
            for (int i = 0; i < ret; i++)
            {

                auto it = _map.find(_events[i].data.fd);
                if (it != _map.end())
                {

                    it->second->SetRevents(_events[i].events);
                    active->push_back(it->second);
                }
                else
                {
                    ERR_LOG("No correct chaneel!");
                }
            }
            return ret;
        }
    }
    // TODO:这里没有释放fd，后面书写时记得补上
    bool RemoveMapDate(int fd)
    {
        // delete _map[fd];
        //这里先不能删除因为这里是又channel发起的close，暂时先不处理
        _map.erase(fd);
        return true;
    }
};
class TimeWheel;
class Test;
using To_Death_Cb = std::function<void()>;
using ReleaseInfo = std::function<bool()>;

class TimeTask
{
private:

    uint64_t _id;           //用于唯一标识任务
    uint32_t _timeout;      //用于标识任务的首次设置后多久死亡
    uint32_t _delay;        //用于表示每次应当延长寿命多久
    uint32_t _dead_index;   //用于表示任务将在哪一秒死亡
    To_Death_Cb _death_task;//用于表示时间到后自动执行的任务
    ReleaseInfo _release;   //这个是回调用于删除map中的信息避免内存泄漏
    bool _cancel;
public:
    TimeTask(const uint64_t id,const uint32_t timeout,uint32_t delay,const uint32_t dead_index ,const To_Death_Cb& death_task)
        :_id(id),_timeout(timeout),_delay(delay),_dead_index(dead_index),_death_task(death_task),_cancel(false){}
    ~TimeTask()
    {

        //任务没有被取消才执行业务回调
        if(_cancel == false && _death_task)  _death_task();
        if(_release)
        _release();
    }
    void SetRelease(ReleaseInfo release) { _release = release; }
    void Cancel() { _cancel = true;}
    uint32_t GetTimeout() const { return _timeout; }
    uint32_t GetStdDelay() const { return _delay; }
    uint32_t GetDeadIndex() const { return _dead_index; }
    void SetDeadTick(uint32_t dead_index) { _dead_index = dead_index; }
};

class EventLoop;
class TimeWheel
{
    //C++ 成员按照声明顺序初始化调用构造,按照声明顺序的逆序调用析构
    using STaskPtr = std::shared_ptr<TimeTask>;
    using WTaskPtr = std::weak_ptr<TimeTask>;
private:
    std::unordered_map<uint64_t,WTaskPtr> _map; //表示id与任务之间的联系
    uint64_t _capacity;                         //表示时间轮的最大容量
    uint32_t _ticks;                            //表示秒针
    std::vector<std::vector<STaskPtr>> _tasks;  //表示所有待办任务
    EventLoop* _ptr;                            //EventLoop* 回指指针
    int _timerfd;
    std::shared_ptr<Channel> _timer_channel;

private:
    //根据id移除_map中的task
    bool RemoveTask(uint64_t id)
    {
       auto it = _map.find(id);
       if(it != _map.end())
       {
           _map.erase(it);
            return true;
       }
       {
           return false;
       }
    }

   void AddTaskInLoop(const uint64_t id,uint32_t timeout,uint32_t delay,const To_Death_Cb& cb)
    {
        if (_capacity == 0)
        {
            ERR_LOG("TimeWheel capacity is zero");
            return;
        }
        //找到对应位置,添加要执行的任务去tasks中
        uint64_t pos = (_ticks + timeout % _capacity) % _capacity;
        STaskPtr ptr = std::make_shared<TimeTask>(id,timeout,delay,pos,cb);
        ptr->SetRelease(std::bind(&TimeWheel::RemoveTask,this,id));
        //这么写是不对的c++标准规定:非静态成员函数(不含this的)函数名不会自动强转成函数指针
        // ptr->SetRelease(std::bind(RemoveTask,this,id));
        _tasks[pos].push_back(ptr);
        // 添加到_map中加以管理
        _map[id] = WTaskPtr(ptr);
    }

    void RefreshInLoop(uint64_t id)
    {
        auto it = _map.find(id);
        if(it != _map.end())
        {
            //计算位置,并添加进入相应位置
            //这是存在问题的，原因在于从weak指针中拿取得原对象，新创建的
            //share_ptr不会共享引用计数
            //_tasks[pos].push_back(STaskPtr(it->second.lock()));
            STaskPtr ptr = it->second.lock();
            if (!ptr)
            {
                _map.erase(it);
                return;
            }
            uint32_t pos = (_ticks + ptr->GetStdDelay()) % _capacity;
            ptr->SetDeadTick(pos);
            _tasks[pos].push_back(ptr);
        }
        else
        {
            //不应该出现的错误,我觉得没什么问题，因为上层已经确定Refresh了，那么这里就应该必然存在否则就是严重代码逻辑错误，但先把exit删了吧
            ERR_LOG("Not find timertaks");
       }
    }

    void CreateTimerFdInLoop(uint32_t sec,uint32_t cycle)
    {
         //进程调用 exec() 启动新程序时，内核自动关闭该 fd，防止 fd 泄漏给子进程。
        struct itimerspec lti;
        memset(&lti, 0, sizeof(struct itimerspec));
        //这个表示下个重复周期，成员表示纳秒和秒
        lti.it_interval.tv_sec = cycle;//
        lti.it_interval.tv_nsec = 0;
        lti.it_value.tv_nsec = 0;
        lti.it_value.tv_sec = sec;
        timerfd_settime(_timerfd, 0, &lti, NULL);
    }

    void CancelTimerTaskInLoop(uint64_t id)
    {
        auto it = _map.find(id);
        if (it == _map.end()) return;

        auto ptr = it->second.lock();
        if (!ptr)
        {
             _map.erase(it);
             return;
        }
        ptr->Cancel();
        _map.erase(it);
    }
public:
    TimeWheel(uint64_t  capacity,EventLoop* ptr)
        :_capacity(capacity)
        ,_ticks(0)
        ,_tasks(capacity)
        ,_ptr(ptr)
        ,_timerfd(timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC))
        ,_timer_channel(std::make_shared<Channel>(_timerfd,_ptr))
        {
            if(_timerfd < 0)
            {
                ERR_LOG("Timerfd create error!");
                exit(ExitCode::TIMERFD_CREATE_ERROR);
            }
            _timer_channel->SetReadCallback(std::bind(&TimeWheel::OnTime,this));
            _timer_channel->EnableRead();
            CreateTimerFd(1, 1);
        }

    //  关于TFD_CLOEXEC:子进程会对父进程的虚拟地址空间进行继承（父子进程同样的页表），只有当对父进程的数据发生操作的时候会发生
    //写时拷贝，修改页表指向其他物理内存地址，子进程退出时会清理进程资源，会保存的时进程退出码（保存在PCB之中task_struct），需要等父进
    //程回收如果子进程继承了fd，文件描述符的涉及是引用计数，fork时其引用计数会+1，子进程不知道该fd的作用可能就不释放，导致fd凭空+1，后续父进程也关不掉了
    //时间到了执行任务
    void OnTime()
    {
        uint64_t n = ReadTimer();
        for(uint64_t i=0;i<n;i++)
            TickMove();
    }

    //sec  : 启动后多少秒之后生效
    //cycle: 再此之后的执行周期为多少秒
    void CreateTimerFd(uint32_t sec,uint32_t cycle);


    //读取timerfd的内容
    uint64_t ReadTimer()
    {
        while(1)
        {
            uint64_t exp = 0;
            ssize_t ret = read(_timerfd,&exp,sizeof(uint64_t));
            if(ret == sizeof(uint64_t))
            {
                //返回本次到期次数，而不是 read 的字节数
                return exp;
            }
            else if(ret < 0 && errno == EINTR)
            {
                //信号中断可重试
                continue;
            }
            else if (ret < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) return 0;
            //发生不应该发生的错误，直接退出即可
            ERR_LOG("Read error!");
            exit(ExitCode::READ_ERROR);

        }
    }

    //刷新事件的生存时间
    void Refresh(uint64_t id);

    //添加一个任务
    void AddTask(const uint64_t id,const uint32_t timeout,uint32_t delay,const To_Death_Cb& cb);

    //tick增加1s,并且处理到时间了的任务
    void TickMove()
    {
        _ticks = (_ticks+1) % _capacity;
        _tasks[_ticks].clear();
    }
    void CancelTimerTask(uint64_t id);
    //这个接口不是线程安全的因此我们规定他只能是只读操作
    bool HasTimer(uint64_t id)
    {
        auto it = _map.find(id);
        if(it != _map.end())
            return true;
        return false;
    }
};

class EventLoop
{
private:
    using Task = std::function<void()>;
private:
    std::vector<Task> _tasks;
    int _eventfd;
    Channel _eventfd_channel;
    Poller _poller;
    std::mutex _mtx;
    std::thread::id _id;
    TimeWheel _time_wheel;
    uint64_t _timeWheelCapacity;
    ssize_t EventFdCallBack()
    {
        uint64_t val;
        int ret = read(_eventfd, &val, sizeof(val));
        if (ret <= 0)
        {
            if (errno == EAGAIN || errno == EINTR)
            {
                return 0;
            }
            else
            {
                ERR_LOG("Read error");
                exit(ExitCode::READ_ERROR);
            }
        }
        return ret;
    }
public:
    EventLoop(uint64_t timerWheelCapacity)
        : _eventfd(eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC))
        , _eventfd_channel(_eventfd, this)
        , _poller()
        , _id(std::this_thread::get_id())
        , _time_wheel(timerWheelCapacity, this)
        , _timeWheelCapacity(timerWheelCapacity)
    {
        _eventfd_channel.SetReadCallback(std::bind(&EventLoop::EventFdCallBack, this));
        _eventfd_channel.EnableRead();
    }
    bool HasTimer(uint64_t id)
    {
       return _time_wheel.HasTimer(id);
    }
    uint64_t GetTimeWheelCapacity() { return _timeWheelCapacity; }
    void UpdateChannel(Channel *channel) { _poller.UpdateEvent(channel);  }
    void RemoveChannel(Channel *channel) { _poller.RemoveListen(channel); }
    void WakeUpThread()
    {
        uint64_t val = 1;
        int ret = write(_eventfd, &val, sizeof(val));
        if (ret < 0)
        {
            ERR_LOG("Write error!");
            exit(ExitCode::WRITE_ERROR);
        }
    }
    void RunInLoop(const Task &task)
    {
        if (std::this_thread::get_id() == _id)
        {
            task();
        }
        else
        {
            QueueInLoop(task);
        }
    }
    void QueueInLoop(const Task &task)
    {
        {
            std::lock_guard<std::mutex> mtx(_mtx);
            _tasks.push_back(task);
        }
        WakeUpThread();
    }
    void RunAllTasks()
    {
        std::vector<Task> tasks; // 内部指向空间为nullptr但是实例是存在的可以使用
        {
            std::lock_guard<std::mutex> mtx(_mtx); // 这个是RAII类型的出了作用域会自动释放锁
            std::swap(tasks, _tasks);              // vector的swap是直接交换指针
        }
        for (Task &t : tasks)
        {
            t();
        }
    }
    void Loop()
    {
        while (true)
        {
            std::vector<Channel *> active;
            // 1.先启动事件监控
            _poller.Poll(&active);
            // 2.处理事件
            for (Channel *connect : active)
            {
                connect->HandleEvent();
            }
            // 3.执行_tasks的任务，涉及线程安全
            // 不循环处理_tasks这是为了线程不要长时间占有锁，影响效率
            RunAllTasks();
        }
    }
    bool IsInThread() { return _id == std::this_thread::get_id(); }

    //时间轮相关

    void AddTimerTask(uint64_t id,const uint32_t timeout,uint32_t delay,const Task& cb)
    {
        _time_wheel.AddTask(id, timeout,delay, cb);
    }
    void RefreshTimer(uint64_t id)
    {
        _time_wheel.Refresh(id);
    }
    void CreateTimerFd(uint32_t sec, uint32_t cycle)
    {
        _time_wheel.CreateTimerFd(sec,cycle);
    }

    void CancelTimerTask(uint64_t id)
    {
        _time_wheel.CancelTimerTask(id);
    }
};


class Connection;
//DISCONNECTED :已关闭;
//CONNECTING   :建立连接但是未处理上下文,半连接
//CONNECTED    :连接已建立可以正常收发上下文;
//DISCONNECTING:连接处于待关闭状态;
using ConnectStatus = enum { DISCONNECTED,CONNECTING,CONNECTED,DISCONNECTING};
using PtrConnection = std::shared_ptr<Connection>;
class Connection : public std::enable_shared_from_this<Connection>
{
private:
    uint64_t _connectId;
    // uint64_t _timerWheelId;
    int _socketfd;
    Socket _socket;
    int _status;
    EventLoop* _loop;
    Channel _channel;
    Any _context;
    Buffer _outBuffer;
    Buffer _inBuffer;
    bool _isPersistentConnect;

private:
    //连接关闭的回调
    using CloseCallBack = std::function<void(const PtrConnection&)>;
    //接收到报文的回回调
    using MessageCallBack = std::function<void(const PtrConnection&,Buffer*)>;
    //任意事件响应的回调
    using AnyEventCallBack = std::function<void(const PtrConnection&)>;
    //连接建立时发生的回调
    using ConnectedCallBack = std::function<void(const PtrConnection&)>;
    CloseCallBack _closeCb;         //提供给用户的释放业务的关闭函数
    MessageCallBack _messageCb;
    AnyEventCallBack _anyEventCb;
    ConnectedCallBack _connectedCb;
    CloseCallBack _serverCloseCb;   //用于管理server服务器中连接的管理

private:
    //关闭该链接的接口

    //绑定到Channel中用于处理回调响应:

    void HandleRead()
    {
        char buffer[65536];
        ssize_t ret = _socket.RecvAsNon(buffer, sizeof(buffer));
        if (ret < 0)
            return ShutdownInLoop();
        if (ret == 0)
            return;
        _inBuffer.Write(buffer, ret);
        if (_messageCb)
            _messageCb(shared_from_this(), &_inBuffer);
    }
    void HandleWrite()
    {
        if (_outBuffer.GetReadableSize() == 0)
        {
            _channel.DisableWrite();
            return;
        }
        ssize_t ret = _socket.SendAsNon(_outBuffer.GetReadPos(), _outBuffer.GetReadableSize());
        if (ret < 0)
        {
            Release();
            return;
        }
        if (ret == 0)
            return;
        _outBuffer.UpdateReadPos(ret);
        if (_outBuffer.GetReadableSize() == 0)
        {
            _channel.DisableWrite();
            if (_status == DISCONNECTING)
                Release();
        }
    }
    void HandleClose()
    {
        if(_inBuffer.GetReadableSize() > 0)
        {
            if(_messageCb) _messageCb(shared_from_this(),&_inBuffer);
        }
        Release();
    }
    //用户设置的回调处理还有定时器的回调处理
    void HandleAnyEvent()
    {
        if (_status == CONNECTED && _isPersistentConnect == false &&
            _loop->HasTimer(_connectId))
            _loop->RefreshTimer(_connectId);
        if (_anyEventCb) _anyEventCb(shared_from_this());
    }
    void HandleError()
    {
        HandleClose();
    }

    //确保任务再同一线程内进行

    //连接建立后要对Connection进行的初始化操作
    void InitConnectionInLoop()
    {
        assert(_status == CONNECTING);
        _status = CONNECTED;
        _channel.EnableRead();
        if(_connectedCb) _connectedCb(shared_from_this());
    }
    void SendInLoop(Buffer &buf)
    {
        if (_status == DISCONNECTED || buf.GetReadableSize() == 0)
            return;
        _outBuffer.WriteAsBuffer(buf);
        if (_channel.WriteAble() == false)
            _channel.EnableWrite();
    }

    void ShutdownInLoop()
    {
        _status = DISCONNECTING;
        //先判断buffer还有没有要处理的数据
        if(_inBuffer.GetReadableSize() > 0)
        {
            if(_messageCb) _messageCb(shared_from_this(),&_inBuffer);//处理inbuffer的数据
        }
        if(_outBuffer.GetReadableSize() > 0)//处理未发送的数据
        {
            if(_channel.WriteAble() == false)
                _channel.EnableWrite();//如果对方已经关闭了，那么读事件会因为错误而关闭整个连接
        }
        if(_outBuffer.GetReadableSize() == 0)//处理完后可以释放连接
            Release();
    }

    void SetNoPersistentConnectInLoop(int sec)
    {
        //设置不是长连接标志位
        _isPersistentConnect = false;
       //理论上不应该出现在时间轮中，因为之前应该是长连接，所以这里写这个是为了代码健壮性？
        if(_loop->HasTimer(_connectId))
            _loop->RefreshTimer(_connectId);
       //添加到时间轮之中
        else
            _loop->AddTimerTask(_connectId, sec, sec, std::bind(&Connection::Release,this));
    }
    void CancelSetNoPersistentConnectInLoop()
    {
        //应该没问题，是长连接那么并且在时间轮中就去取消它进行业务关闭只进行自己的任务关闭，没有的话就不要管了
        _isPersistentConnect = true;
        if(_loop->HasTimer(_connectId))
            _loop->CancelTimerTask(_connectId);
    }
    void UpgradeInLoop(const Any& context,const CloseCallBack& closeCb,
        const MessageCallBack& messageCb,const AnyEventCallBack& anyEventCb,
        const ConnectedCallBack& connectedCb)
    {
        _connectedCb = connectedCb;
        _messageCb = messageCb;
        _anyEventCb = anyEventCb;
        _context = context;
        _closeCb = closeCb;
    }

    void ReleaseInLoop()
    {
        //由于很多回调事件都有可能触发ReleaseInLoop，所以这里设置一个状态检测来避免二次释放
        if (_status == DISCONNECTED)
            return;

        // self 保证回调里即使从 _map 删除最后一个 shared_ptr，本函数后续也不会悬空
        auto self = shared_from_this();
        _status = DISCONNECTED;
        _channel.Remove();
        _socket.Close();
        if (_loop->HasTimer(_connectId))
            CancelSetNoPersistentConnectInLoop();

        if (_closeCb) _closeCb(self);
        if (_serverCloseCb) _serverCloseCb(self);
    }
public:

    void SetServerCloseCallBack(CloseCallBack serverCb)
    {
        _serverCloseCb = serverCb;
    }

    Connection(EventLoop* loop,int socketfd,uint64_t id)
        :_connectId(id)
        ,_socketfd(socketfd)
        ,_socket(_socketfd)
        ,_status(CONNECTING)
        ,_loop(loop)
        ,_channel(_socketfd,_loop)
        ,_isPersistentConnect(false)
    {
        // _channel.EnableRead();这里不能启动读事件监控因为一旦启动epoll开始监控但是各回调事件都还没初始化完毕
        _channel.SetCloseCallback(std::bind(&Connection::HandleClose,this));
        _channel.SetReadCallback(std::bind(&Connection::HandleRead,this));
        _channel.SetWriteCallback(std::bind(&Connection::HandleWrite,this));
        _channel.SetErrorCallback(std::bind(&Connection::HandleError,this));
        _channel.SetEventCallback(std::bind(&Connection::HandleAnyEvent,this));
    }
    ~Connection()
    {

    }

    int GetFd() { return _socketfd; }
    uint64_t GetId() { return _connectId; }
    Any* GetContext() { return  &_context; }
    void SetContext(const Any& context) { _context = context; }
    void SetCloseCallBack(const CloseCallBack& closeCb)             { _closeCb = closeCb; }
    void SetMessageCallBack(const MessageCallBack& messageCb)       { _messageCb = messageCb;}
    void SetAnyEventCallback(const AnyEventCallBack& anyEventCb)    { _anyEventCb = anyEventCb; }
    void SetConnectedCallBack(const ConnectedCallBack& connectedCb) { _connectedCb = connectedCb;}
    void SetIsOrNotPersistent(bool flag) { _isPersistentConnect = flag; }
    void Send(const char *data, size_t len)
    {
        Buffer buf;
        buf.Write(data, len);
        auto self = shared_from_this();
        _loop->RunInLoop([self, buf = std::move(buf)]() mutable {
            self->SendInLoop(buf);
        });
    }

    //切换协议重置上下文
    void Upgrade(const Any& context,const CloseCallBack& closeCb,
        const MessageCallBack& messageCb,const AnyEventCallBack& anyEventCb,
        const ConnectedCallBack& connectedCb)
    {
        _loop->RunInLoop(std::bind(&Connection::UpgradeInLoop,this,context,closeCb,messageCb,anyEventCb,connectedCb));
    }
    //指的是将连接置于待关闭状态
    void Shutdown()
    {
        _loop->RunInLoop(std::bind(&Connection::ShutdownInLoop,this));
    }
    //设置该连接为非长连接并在sec秒后关闭
    void SetNoPersistentConnect(int sec)
    {
        _loop->RunInLoop(std::bind(&Connection::SetNoPersistentConnectInLoop,this,sec));
    }
    void CancelSetNoPersistentConnect()
    {
        _loop->RunInLoop(std::bind(&Connection::CancelSetNoPersistentConnectInLoop,this));
    }

    void Release()
    {
        auto self = shared_from_this();
        _loop->QueueInLoop([self]() { self->ReleaseInLoop(); });
    }
    void InitConnection()
    {
        _loop->RunInLoop(std::bind(&Connection::InitConnectionInLoop,this));
    }
};
//主线程进行监听fd的模块
class Acceptor
{
private:
    EventLoop* _baseLoop;
    Channel _acceptorChannel;
    Socket _socket;
private:
    using AcceptorCallBack = std::function<void(int)>;
    AcceptorCallBack _acceptorCb;
public:
    Acceptor(EventLoop* baseLoop,const uint16_t port)
        :_baseLoop(baseLoop)
        ,_acceptorChannel(-1,_baseLoop)
        ,_socket()
    {
        _socket.CreateServer("0.0.0.0", port);
        _acceptorChannel.SetFd(_socket.GetSocket());
        _acceptorChannel.SetReadCallback(std::bind(&Acceptor::HandleRead,this));
    }
    void HandleRead()
    {
        while(true)
        {
            int fd = _socket.AcceptNewLink();
            if(fd < 0 && (errno == EAGAIN))
            {
                return;
            }
            if(fd < 0 && errno == EINTR) continue;
            if(fd < 0) return;
            if(_acceptorCb) _acceptorCb(fd);//暂时定为业务处理函数
        }

    }

    void Listen()
    {

        //可读的设置必须在回调处理完成之后
        _acceptorChannel.SetReadCallback(std::bind(&Acceptor::HandleRead,this));
        _acceptorChannel.EnableRead();
    }

    void SetAcceptorCallback(const AcceptorCallBack& acceptorCb)
    {
        _acceptorCb = acceptorCb;
    }

};

//类内声明的自定义类会是会调用其默认构造的不管有没有写在初始化列表，而内置类型（int，float等）是不会自动初始化的需要手动

//线程启动后的调度是无规律的，也就是说你先起来的线程不一定先执行，所以用LoopThread来保障EventLoop的同步
class LoopThread
{
private:

    std::mutex _mtx;
    std::condition_variable _cond;
    EventLoop* _loop;
    std::thread _thread;
public:
    LoopThread(uint64_t capacity)
        :_mtx()
        ,_cond()
        ,_loop(nullptr)
        ,_thread(std::bind(&LoopThread::ThreadEntry,this,capacity))
    {

    }
    void ThreadEntry(uint64_t capacity)
    {
        EventLoop loop(capacity);
        {
            std::unique_lock<std::mutex> mtx(_mtx);
            _loop = &loop;
            _cond.notify_all();
        }
        loop.Loop();
    }
    EventLoop* GetEventLoop()
    {
        EventLoop* loop = nullptr;
        {
            std::unique_lock<std::mutex> mtx(_mtx);
            _cond.wait(mtx,[&](){ return _loop != nullptr;});
            loop = _loop;
        }
        return loop;
    }

};

class ThreadPool
{
private:
    uint64_t _threadNums;//从属线程数量
    EventLoop* _baseLoop;//主线程
    std::vector<LoopThread*> _loopThreads;//管理LoopThread
    std::vector<EventLoop*> _eventLoops;
    uint64_t _index;//根据RR轮转判断这次要分配给哪一个线程

private:

public:
    ThreadPool(EventLoop* loop)
        :_threadNums(0)
        ,_baseLoop(loop)
        ,_index(0)
    {

    }
    void SetThreadNums(uint64_t nums)
    {
        _threadNums =  nums;
    }
    void CreateAllThreads()
    {
        if(_threadNums > 0)
        {
            for(uint64_t i = 0;i<_threadNums;i++)
            {
                _loopThreads.push_back(new LoopThread(_baseLoop->GetTimeWheelCapacity()));
                // _loopThreads[i] = new LoopThread(30);这是不正确的[]只会进行下标访问不会进行自动扩容
                _eventLoops.push_back(_loopThreads[i]->GetEventLoop());
                // _eventLoops[i] = _loopThreads[i]->GetEventLoop();
            }
        }
        return;
    }

    EventLoop* GetNextLoop()
    {
        if(_threadNums == 0 || _eventLoops.size() < _threadNums)
            return _baseLoop;
        _index = (_index + 1) % _threadNums;
        return _eventLoops[_index];
    }

};

class TcpServer
{
private:
    uint64_t _id;
    uint64_t _nextTimerId;
    uint16_t _port;
    uint64_t _timeout;
    bool _enable_inactive_release;
    EventLoop _baseLoop;
    Acceptor _acceptor;
    ThreadPool _threadPool;
    std::unordered_map<uint64_t,PtrConnection> _map;

private:
    //连接关闭的回调
    using CloseCallBack = std::function<void(const PtrConnection&)>;
    //接收到报文的回回调
    using MessageCallBack = std::function<void(const PtrConnection&,Buffer*)>;
    //任意事件响应的回调
    using AnyEventCallBack = std::function<void(const PtrConnection&)>;
    //连接建立时发生的回调
    using ConnectedCallBack = std::function<void(const PtrConnection&)>;
    using Task = std::function<void()>;
    CloseCallBack _closeCb;
    MessageCallBack _messageCb;
    AnyEventCallBack _anyEventCb;
    ConnectedCallBack _connectedCb;
    CloseCallBack _serverCloseCb;
private:
    void RemoveConnectionInLoop(const PtrConnection& conn)
    {
        _baseLoop.RunInLoop(std::bind(&TcpServer::RemoveConnection,this,conn));
    }
    //这个函数是对Connections集合进行管理的进行connection的释放
    void RemoveConnection(const PtrConnection& conn)
    {
        _map.erase(conn->GetId());
    }

    void NewConnection(int fd)
    {
        //构造一个连接并且关联到对应的EventLoop线程
        PtrConnection newConnection = std::make_shared<Connection>(_threadPool.GetNextLoop(),fd,_id);
        //设置回调函数
        newConnection->SetAnyEventCallback(_anyEventCb);
        newConnection->SetCloseCallBack(_closeCb);
        newConnection->SetConnectedCallBack(_connectedCb);
        newConnection->SetMessageCallBack(_messageCb);
        if (_enable_inactive_release)
            newConnection->SetNoPersistentConnect(_timeout);
        newConnection->SetServerCloseCallBack(std::bind(&TcpServer::RemoveConnectionInLoop, this, std::placeholders::_1));
        //添加到_map中进行管理

        _map[_id] = newConnection;
        _id++;
        newConnection->InitConnection();
    }

    void AddTimerTaskInLoop(const Task& task,uint64_t timeout,uint64_t delay)
    {
        _baseLoop.AddTimerTask(++_nextTimerId,timeout,delay,task);
    }

    void CancelTimerTaskInLoop(uint64_t id)
    {
        _baseLoop.CancelTimerTask(id);
    }

public:
    //baseloop的定时器任务和其他eventloop的性质是不大一样的，他主要是一些其他任务的管理
    void AddTimerTask(const Task& task,uint64_t timeout,uint64_t delay)
    {
        _baseLoop.RunInLoop(std::bind(&TcpServer::AddTimerTaskInLoop,this,task,timeout,delay));
    }

    void CancelTimerTask(uint64_t _id)
    {
       _baseLoop.RunInLoop(std::bind(&TcpServer::CancelTimerTaskInLoop,this,_id));
    }

    //capacity:timewheel的大小
    TcpServer(const int capacity,const uint16_t port)
        :_id(0)
        ,_nextTimerId(0x8000000000000000ULL)
        ,_port(port)
        ,_timeout(10)
        ,_enable_inactive_release(false)
        ,_baseLoop(capacity)
        ,_acceptor(&_baseLoop,_port)
        ,_threadPool(&_baseLoop)
    {
        _acceptor.SetAcceptorCallback(std::bind(&TcpServer::HandleAcceptorCallBack,this,std::placeholders::_1));
        _acceptor.Listen();

    }

    void SetThreadPoolNums(const int nums) { _threadPool.SetThreadNums(nums); }
    void SetCloseCallBack(const CloseCallBack& closeCb)             { _closeCb = closeCb; }
    void SetMessageCallBack(const MessageCallBack& messageCb)       { _messageCb = messageCb;}
    void SetAnyEventCallback(const AnyEventCallBack& anyEventCb)    { _anyEventCb = anyEventCb; }
    void SetConnectedCallBack(const ConnectedCallBack& connectedCb) { _connectedCb = connectedCb;}
    void EnableInactiveRelease(int timeout)
    {
        if (timeout <= 0)
            return;
        _timeout = timeout;
        _enable_inactive_release = true;
    }

    void HandleAcceptorCallBack(int fd)
    {
        NewConnection(fd);
    }

    //服务启动
    void Start()
    {
        _threadPool.CreateAllThreads();
        _baseLoop.Loop();
    }
};

inline void Channel::Update() { _ptr->UpdateChannel(this); }
inline void Channel::Remove() { _ptr->RemoveChannel(this); }


inline void TimeWheel::AddTask(const uint64_t id,const uint32_t timeout,uint32_t delay,const To_Death_Cb& cb)
{
    _ptr->RunInLoop(std::bind(&TimeWheel::AddTaskInLoop,this,id,timeout,delay,cb));
}
inline void TimeWheel::Refresh(uint64_t id)
{
    _ptr->RunInLoop(std::bind(&TimeWheel::RefreshInLoop,this,id));
}
inline void TimeWheel::CreateTimerFd(uint32_t sec,uint32_t cycle)
{
    _ptr->RunInLoop(std::bind(&TimeWheel::CreateTimerFdInLoop,this,sec,cycle));
}
inline void TimeWheel::CancelTimerTask(uint64_t id)
{
    _ptr->RunInLoop(std::bind(&TimeWheel::CancelTimerTaskInLoop,this,id));
}