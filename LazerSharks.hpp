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

        virtual int write(const char *buf, int len);

        const Kite::InternetAddress &address() const;

    protected:
        virtual void onActivated(int fd, int events) override final;
        virtual void onClosing() override final;
    private:
        Stack *stack;
        class Private;
        Private *d;
    };

    typedef std::function<void(LazerSharks::Handle &)> Middleware;

    class Stack {
    public:
        Kite::TcpServer::Factory tcpFactory();
        void call(const Middleware &m);

    private:
        std::vector<Middleware> d_middleware;
        std::vector<Handle *> d_handles;
        friend class Handle;
    };
};

#endif
