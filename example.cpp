#include <Kite/EventLoop.hpp>
#include <Kite/Process.hpp>
#include "LazerSharks.hpp"

int main(int argc, char **argv)
{
    std::shared_ptr<Kite::EventLoop> ev(new Kite::EventLoop);
    std::shared_ptr<LazerSharks::Stack> lz(new LazerSharks::Stack);
    std::shared_ptr<Kite::TcpServer> s(new Kite::TcpServer(ev, lz->tcpFactory()));


    lz->call([](LazerSharks::Handle &r) {


            Kite::Process::shell("ubus call dhcp ipv4leases");

            std::string s = "yo mate!\nyou callin from " + r.address().address();
            r.write(s.c_str(), s.size());
    });

    if (!s->listen(Kite::InternetAddress(Kite::InternetAddress::Any, 8000))) {
        return 3;
    }

    std::cerr << "listening on port 8000" << std::endl;

    return ev->exec();
}

