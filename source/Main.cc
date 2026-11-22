/*
 * @Author: HejieY 2145381080@qq.com
 * @Date: 2026-10-27 17:03:28
 * @FilePath: /muduo-http-server/source/Main.cc
 * @Description:
 * Copyright (c) 2026 by HeJieY, All Rights Reserved.
 */
/*
 * @Author: HejieY 2145381080@qq.com
 * @Date: 2026-10-19 14:36:19
 * @FilePath: /muduo-http-server/source/Main.cc
 * @Description:
 * Copyright (c) 2026 by HeJieY, All Rights Reserved.
 */
#include "Server.hpp"
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <functional>
#include <sys/socket.h>
#include <unistd.h>
#include<iostream>
#include <signal.h>
class EchoServer
{
private:
    TcpServer _server;
public:
    EchoServer(uint64_t capacity,uint16_t port)
        :_server(capacity, port)
    {
        _server.SetCloseCallBack(std::bind(&EchoServer::OnClose,this,std::placeholders::_1));
        _server.SetConnectedCallBack(std::bind(&EchoServer::OnConnect,this,std::placeholders::_1));
        _server.SetMessageCallBack(std::bind(&EchoServer::OnMessage,this,std::placeholders::_1,std::placeholders::_2));
    
    }
    ~EchoServer()
    {
        
    }
    void Start()
    {
        _server.Start();
    }
    //处理的是inbuffer到outbuffer的数据
    void OnMessage(const PtrConnection& conn,Buffer* inbuffer)
    {
        int len = inbuffer->GetReadableSize();
        // inbuffer->UpdateReadPos(len);因为下面asstring更新了
        conn->Send(inbuffer->GetReadPos(), len);
        
        std::cout << "client say#"<<inbuffer->ReadAsString(len) << std::endl;
        
    }
    void OnClose(const PtrConnection& conn)
    {
        printf("Close a link,%d,%ld\n",conn->GetFd(),conn->GetId());
    }
    void OnConnect(const PtrConnection& conn)
    {
        printf("Get a link!%d,%ld",conn->GetFd(),conn->GetId());
    }
    void SetThreadNums(uint64_t n)
    {
        _server.SetThreadPoolNums(n);
    }

};

int main()
{
    signal(SIGPIPE, SIG_IGN);//这个是为了服务器发送给客户端但客户端断开连接时避免被线程中断的
    EchoServer echo(60,8081);
    echo.SetThreadNums(5);
    echo.Start();
    return 0;
}
