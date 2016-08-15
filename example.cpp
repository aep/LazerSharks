#include <Kite/EventLoop.hpp>
#include <Kite/Process.hpp>
#include "LazerSharks.hpp"

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

LazerSharks::Handle &http_index(LazerSharks::Handle &r) {
    if (r.requestUrl != "/index.html") {
        return r.next();
    }

    std::string s = "yo mate!\nyou callin from " + r.requestAddress.address();
    s += "<ul>";
    s += "<li><a href='/example.cpp'> here is some source code</a></li>";
    s += "<li><a href='/nah'> here's a 404</a></li>";
    s += "</ul>";

    return r.respond(200)
        .header("content-type", "text/html")
        .body(s);
}

LazerSharks::Handle &http_static(LazerSharks::Handle &r) {
    if (r.requestUrl.find("..") != std::string::npos) {
        return r.respond(404);
    }

    int fd = open(("./" + r.requestUrl).c_str(), O_RDONLY);
    if (fd < 1) {
        return r.next();
    }
    std::cerr << "http_static " << r.requestUrl << std::endl;

    r.respond(200);
    for(;;) {
        char buf[1028];
        int l = ::read(fd, buf, 1024);
        r.write(buf, l);
        if (l < 1) {
            ::close(fd);
            return r;
        }
    }
}

int main(int argc, char **argv)
{
    std::shared_ptr<Kite::EventLoop> ev(new Kite::EventLoop);
    std::shared_ptr<LazerSharks::Stack> lz(new LazerSharks::Stack);
    std::shared_ptr<Kite::TcpServer> s(new Kite::TcpServer(ev, lz->tcpFactory()));


    //just some logging
    lz->call([](LazerSharks::Handle &r) -> LazerSharks::Handle & {
            std::cerr << r.requestUrl  << std::endl;
            return r.next();
            });

    //handle / as /index.html
    lz->call([](LazerSharks::Handle &r) -> LazerSharks::Handle &{
            if (r.requestUrl == "/") {
               r.requestUrl = "/index.html";
            }
            return r.next();
            });

    lz->call(http_index);
    lz->call(http_static);

    if (!s->listen(Kite::InternetAddress(Kite::InternetAddress::Any, 8000))) {
        return 3;
    }

    std::cerr << "listening on port 8000" << std::endl;

    return ev->exec();
}

