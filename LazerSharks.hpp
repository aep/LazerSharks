#ifndef LazerSharks_HPP
#define LazerSharks_HPP
#include <iostream>
#include "Kite/EventLoop.hpp"
#include "Kite/File.hpp"
#include "Kite/TcpServer.hpp"
#include "Kite/Scope.hpp"
#include "Kite/Internet.hpp"

namespace LazerSharks {

    class Stack;


    class Handle : public Kite::TcpConnection , public Kite::Scope {
    public:
        Handle(Stack *stack, std::weak_ptr<Kite::EventLoop> ev, int fd, const Kite::InternetAddress &address);
        ~Handle();

        virtual int write(const char *buf, int len) override;

        //returnable.
        Handle &next();
        Handle &respond (int code);
        Handle &body    (const char *buf, int len);
        Handle &body    (const std::string&);
        Handle &header  (const std::string &key, const std::string &val);

        // request. can be modified by middleware
        std::map<std::string,std::string> requestHeaders;
        Kite::InternetAddress requestAddress;
        std::string requestUrl;

        // response. can be modified by middleware
        std::string responseStatus;
        std::map<std::string,std::string> responseHeaders;

    protected:
        virtual void onActivated(int fd, int events) override final;
        virtual void onClosing() override final;
    private:
        Stack *stack;
        class Private;
        Private *d;
    };

    typedef std::function<LazerSharks::Handle &(LazerSharks::Handle &)> Middleware;

    class Stack {
    public:
        Kite::TcpServer::Factory tcpFactory();
        void call(const Middleware &m);

    private:
        std::queue<Middleware> d_middleware;
        std::vector<Handle *> d_handles;
        friend class Handle;
    };
};

#endif
