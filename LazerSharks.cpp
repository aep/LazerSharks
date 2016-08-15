#include "LazerSharks.hpp"

extern "C" {
#include "3rdparty/http-parser/http_parser.h"
}
#include <string.h>

Kite::TcpServer::Factory LazerSharks::Stack::tcpFactory()
{
    return [this](std::weak_ptr<Kite::EventLoop> ev, int fd, const Kite::InternetAddress &address) {
        auto r = new LazerSharks::Handle(this, ev, fd, address);
        d_handles.push_back(r);
    };
}

void LazerSharks::Stack::call(const LazerSharks::Middleware &middleware) {
    d_middleware.push_back(middleware);
}


class LazerSharks::Handle::Private {
public:
    Handle *p;
    http_parser_settings settings;
    http_parser parser;
    Kite::InternetAddress address;

    static int http_on_url(http_parser* parser, const char *at, size_t length);
    static int http_on_header_field(http_parser* parser, const char *at, size_t length);
    static int http_on_header_value(http_parser* parser, const char *at, size_t length);
    static int http_on_headers_complete(http_parser* parser);
    static int http_on_body(http_parser* parser, const char *at, size_t length);

    std::string url;

    std::string r_status;
    bool r_has_written_headers;
};

LazerSharks::Handle::Handle(Stack *stack, std::weak_ptr<Kite::EventLoop> ev, int fd, const Kite::InternetAddress &address)
    : Kite::TcpConnection(ev, fd)
    , stack(stack)
    , d(new Private)
{
    d->p = this;
    d->r_has_written_headers = false;
    d->r_status = "200 OK";
    d->address = address;
    memset(&d->settings, 0, sizeof(http_parser_settings));

    d->settings.on_url          = LazerSharks::Handle::Private::http_on_url;
    d->settings.on_header_field = LazerSharks::Handle::Private::http_on_header_field;
    d->settings.on_header_value = LazerSharks::Handle::Private::http_on_header_value;
    d->settings.on_headers_complete = LazerSharks::Handle::Private::http_on_headers_complete;
    d->settings.on_body = LazerSharks::Handle::Private::http_on_body;

    http_parser_init(&d->parser, HTTP_REQUEST);
    d->parser.data = d;
}

LazerSharks::Handle::~Handle()
{
    stack->d_handles.erase(std::remove(stack->d_handles.begin(),
                stack->d_handles.end(), this), stack->d_handles.end());
    delete d;
}

const Kite::InternetAddress &LazerSharks::Handle::address() const
{
    return d->address;
}

void LazerSharks::Handle::onClosing()
{
    ev().lock()->deleteLater(this);
}


int LazerSharks::Handle::Private::http_on_url(http_parser* parser, const char *at, size_t length)
{
    LazerSharks::Handle::Private *d = (LazerSharks::Handle::Private *)parser->data;
    d->url = std::string(at, length);
    return 0;
}

int LazerSharks::Handle::Private::http_on_header_field(http_parser* parser, const char *at, size_t length)
{
    return 0;
}

int LazerSharks::Handle::Private::http_on_header_value(http_parser* parser, const char *at, size_t length)
{
    return 0;
}

int LazerSharks::Handle::Private::http_on_headers_complete(http_parser* parser)
{
    LazerSharks::Handle::Private *d = (LazerSharks::Handle::Private *)parser->data;
    for (auto middleware : d->p->stack->d_middleware) {
        middleware(*d->p);
    }
    d->p->close();
    return 0;
}

int LazerSharks::Handle::Private::http_on_body(http_parser* parser, const char *at, size_t length)
{
    return 0;
}

void LazerSharks::Handle::onActivated(int fd, int events)
{
    char buf[1024];
    int r = read(buf, 1024);
    if (r < 1)
        return;
    http_parser_execute(&d->parser, &d->settings, buf, r);
}

int LazerSharks::Handle::write(const char *buf, int len)
{
    if (!d->r_has_written_headers) {
        d->r_has_written_headers = true;
        std::string s =
            "HTTP/1.1 " + d->r_status + "\n" +
            "Connection: close\n\n";
        Kite::TcpConnection::write(s.c_str(), s.size());
    }
    return Kite::TcpConnection::write(buf, len);
}
