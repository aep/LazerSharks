#include <Kite/EventLoop.hpp>
#include "LazerSharks.hpp"

int main(int argc, char **argv)
{
    std::shared_ptr<Kite::EventLoop> ev(new Kite::EventLoop);
    std::shared_ptr<LazerSharks::Stack> lz(new LazerSharks::Stack);
    std::shared_ptr<Kite::TcpServer> s(new Kite::TcpServer(ev, lz->tcpFactory()));


    lz->call([](LazerSharks::Handle &r) {
            std::string s = "yo mate!";
            r.write(s.c_str(), s.size());
    });

    if (!s->listen(Kite::InternetAddress(Kite::InternetAddress::Any, 8000))) {
        return 3;
    }

    std::cerr << "listening on port 8000" << std::endl;

    return ev->exec();
}

