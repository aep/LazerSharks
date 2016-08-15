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
    d_middleware.push(middleware);
}


class LazerSharks::Handle::Private {
public:
    Handle *p;
    Kite::Scope *later;
    http_parser_settings settings;
    http_parser parser;
    std::string parserB;

    static int http_on_url(http_parser* parser, const char *at, size_t length);
    static int http_on_header_field(http_parser* parser, const char *at, size_t length);
    static int http_on_header_value(http_parser* parser, const char *at, size_t length);
    static int http_on_headers_complete(http_parser* parser);
    static int http_on_body(http_parser* parser, const char *at, size_t length);

    std::map<std::string, std::string> responseHeaders;
    bool r_has_written_headers;

    std::queue<Middleware> middleware;
};

LazerSharks::Handle::Handle(Stack *stack, std::weak_ptr<Kite::EventLoop> ev, int fd, const Kite::InternetAddress &address)
    : Kite::TcpConnection(ev, fd)
    , stack(stack)
    , d(new Private)
{
    d->p = this;
    d->later = 0;
    d->r_has_written_headers = false;
    responseStatus = "404 NOT FOUND";
    requestAddress = address;

    memset(&d->settings, 0, sizeof(http_parser_settings));
    d->settings.on_url          = LazerSharks::Handle::Private::http_on_url;
    d->settings.on_header_field = LazerSharks::Handle::Private::http_on_header_field;
    d->settings.on_header_value = LazerSharks::Handle::Private::http_on_header_value;
    d->settings.on_headers_complete = LazerSharks::Handle::Private::http_on_headers_complete;
    d->settings.on_body = LazerSharks::Handle::Private::http_on_body;

    http_parser_init(&d->parser, HTTP_REQUEST);
    d->parser.data = d;
    d->middleware = stack->d_middleware;
}

LazerSharks::Handle::~Handle()
{
    stack->d_handles.erase(std::remove(stack->d_handles.begin(),
                stack->d_handles.end(), this), stack->d_handles.end());
    delete d;
}

void LazerSharks::Handle::onClosing()
{
    ev().lock()->deleteLater(this);
    if (d->later) {
        ev().lock()->deleteLater(d->later);
    }
}

int LazerSharks::Handle::Private::http_on_url(http_parser* parser, const char *at, size_t length)
{
    LazerSharks::Handle::Private *d = (LazerSharks::Handle::Private *)parser->data;
    d->p->requestUrl = std::string(at, length);
    d->p->url = d->p->requestUrl;
    return 0;
}

int LazerSharks::Handle::Private::http_on_header_field(http_parser* parser, const char *at, size_t length)
{
    LazerSharks::Handle::Private *d = (LazerSharks::Handle::Private *)parser->data;
    d->parserB = std::string(at, length);
    return 0;
}

int LazerSharks::Handle::Private::http_on_header_value(http_parser* parser, const char *at, size_t length)
{
    LazerSharks::Handle::Private *d = (LazerSharks::Handle::Private *)parser->data;
    d->p->requestHeaders[d->parserB] = std::string(at, length);
    return 0;
}

int LazerSharks::Handle::Private::http_on_headers_complete(http_parser* parser)
{
    LazerSharks::Handle::Private *d = (LazerSharks::Handle::Private *)parser->data;
    d->p->next();
    if (!d->later) {
        d->p->write(0,0);
        d->p->close();
    }
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
            "HTTP/1.1 " + responseStatus + "\n" +
            "Connection: close\n";
        for (auto h : responseHeaders) {
            s+= h.first + ":" + h.second + "\n";
        }
        s+="\n";
        Kite::TcpConnection::write(s.c_str(), s.size());

    }
    return Kite::TcpConnection::write(buf, len);
}

LazerSharks::Handle &LazerSharks::Handle::respond(int ret)
{
    switch (ret) {
        case 200:
            d->p->responseStatus = "200 OK";
            break;
        case 404:
            d->p->responseStatus = "404 NOT FOUND";
            break;
        default:
            d->p->responseStatus = std::to_string(ret) + " OTHER";
            break;
    };
    return *this;
}

LazerSharks::Handle &LazerSharks::Handle::body(const std::string &s)
{
    write(s.c_str(), s.size());
    return *this;
}

LazerSharks::Handle &LazerSharks::Handle::body(const char *buf, int len)
{
    write(buf, len);
    return *this;
}
LazerSharks::Handle &LazerSharks::Handle::header(const std::string &key, const std::string &val)
{
    responseHeaders[key] = val;
    return *this;
}

LazerSharks::Handle &LazerSharks::Handle::next()
{
    if (d->middleware.empty()) {
        return d->p->respond(404).body("no middleware accepted this request");
    }
    auto m = d->middleware.front();
    d->middleware.pop();
    return m(*d->p);
}

LazerSharks::Handle &LazerSharks::Handle::later   (Kite::Scope *sc)
{
    if (d->later) {
        throw std::runtime_error("LazerSharks::Handle:later called twice");
    }
    setNotificationScope(sc);
    d->later = sc;
}

void LazerSharks::Handle::onDeathNotify(Scope *sc)
{
    close();
}

