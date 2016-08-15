#include <Kite/EventLoop.hpp>
#include <Kite/HttpClient.hpp>
#include <Kite/Process.hpp>
#include "LazerSharks.hpp"

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

LazerSharks::Handle &http_index(LazerSharks::Handle &r) {
    if (r.url != "/index.html") {
        return r.next();
    }

    std::string s = "yo mate!\nyou callin from " + r.requestAddress.address();
    s += "<ul>";
    s += "<li><a href='/example.cpp'> here is some source code</a></li>";
    s += "<li><a href='/nah'> here's a 404</a></li>";
    s += "<li><a href='/proxy'> async proxy request</a></li>";
    s += "</ul>";

    return r.respond(200)
        .header("content-type", "text/html")
        .body(s);
}

LazerSharks::Handle &http_static(LazerSharks::Handle &r) {
    if (r.url.find("..") != std::string::npos) {
        return r.respond(404);
    }

    int fd = open(("./" + r.url).c_str(), O_RDONLY);
    if (fd < 1) {
        return r.next();
    }
    std::cerr << "http_static " << r.url << std::endl;

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


class ProxyRequest : public Kite::HttpClient
{
public:
    ProxyRequest(LazerSharks::Handle &r)
        : Kite::HttpClient(r.ev())
        , r(r)
    {
        get("http://www.heise.de");
    }
    LazerSharks::Handle &r;
protected:
    virtual void onHeadersReady(int code, const std::map<std::string,std::string> &responseHeaders) override
    {
        r.responseHeaders = responseHeaders;
        r.respond(code);
    }

    virtual void onReadActivated() override
    {
        char buf[1024];
        int l = read(buf, 1024);
        r.body(buf, l);
    }
    virtual void onFinished(Status status, int responseCode, const std::string &body) override
    {
        r.respond(responseCode);
        if (responseCode != 200) {
            r.body(errorMessage());
        }
        r.respond(responseCode).body(body).close();
    }
};

LazerSharks::Handle &http_proxy_proxy(LazerSharks::Handle &r) {
    if (r.url != "/proxy") {
        return r.next();
    }

    return r.later(new ProxyRequest(r));
}

int main(int argc, char **argv)
{
    std::shared_ptr<Kite::EventLoop> ev(new Kite::EventLoop);
    std::shared_ptr<LazerSharks::Stack> lz(new LazerSharks::Stack);
    std::shared_ptr<Kite::TcpServer> s(new Kite::TcpServer(ev, lz->tcpFactory()));

    //just some logging
    lz->call([](LazerSharks::Handle &r) -> LazerSharks::Handle & {
            std::cerr << r.url  << std::endl;
            return r.next();
            });

    //handle / as /index.html
    lz->call([](LazerSharks::Handle &r) -> LazerSharks::Handle &{
            if (r.url == "/") {
               r.url = "/index.html";
            }
            return r.next();
            });

    lz->call(http_index);
    lz->call(http_proxy_proxy);
    lz->call(http_static);

    if (!s->listen(Kite::InternetAddress(Kite::InternetAddress::Any, 8000))) {
        return 3;
    }

    std::cerr << "listening on port 8000" << std::endl;

    return ev->exec();
}

