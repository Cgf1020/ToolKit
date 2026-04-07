#include "Http.h"

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/error.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/version.hpp>
#include <boost/url/parse.hpp>

#include "root_certificates.hpp"

namespace util::Http {
    void transfer(std::string const& method, std::string const& url, std::map<std::string, std::string> const& headers, std::string const& body,
        std::function<void(std::string&& r)>&& postProcessor) noexcept {
        try {
            // The io_context is required for all I/O
            boost::asio::io_context ioc;
            // These objects perform our I/O
            boost::asio::ip::tcp::resolver resolver(ioc);
            boost::urls::result<boost::urls::url_view> r = boost::urls::parse_uri(url);
            if (!r.has_value()) {
                postProcessor("");
                return;
            }
            auto u = r.value();
            auto s = u.scheme();
            auto host = u.encoded_host();
            auto port = u.port();
            if (port == "") {
                port = s == "https" ? "https" : "http";
            }
            auto target = u.encoded_target();
            // Look up the domain name
            auto const results = resolver.resolve(host, port);
            // Set up an HTTP GET request message
            constexpr int version = 11;
            boost::beast::http::verb httpMethod = boost::beast::http::verb::unknown;
            if (method == "GET") {
                httpMethod = boost::beast::http::verb::get;
            } else if (method == "POST") {
                httpMethod = boost::beast::http::verb::post;
            } else if (method == "PUT") {
                httpMethod = boost::beast::http::verb::put;
            } else if (method == "DELETE") {
                httpMethod = boost::beast::http::verb::delete_;
            } else if (method == "HEAD") {
                httpMethod = boost::beast::http::verb::head;
            } else if (method == "PATCH") {
                httpMethod = boost::beast::http::verb::patch;
            } else if (method == "PURGE") {
                httpMethod = boost::beast::http::verb::purge;
            } else if (method == "LINK") {
                httpMethod = boost::beast::http::verb::link;
            } else if (method == "UNLINK") {
                httpMethod = boost::beast::http::verb::unlink;
            } else if (method == "CONNECT") {
                httpMethod = boost::beast::http::verb::connect;
            } else if (method == "OPTIONS") {
                httpMethod = boost::beast::http::verb::options;
            } else if (method == "TRACE") {
                httpMethod = boost::beast::http::verb::trace;
            }
            boost::beast::http::request<boost::beast::http::string_body> req{httpMethod, target, version};
            for (auto const& header : headers) {
                req.insert(header.first, header.second);
            }
            req.set(boost::beast::http::field::host, host);
            req.set(boost::beast::http::field::user_agent, BOOST_BEAST_VERSION_STRING);
            if (body != "") {
                req.set(boost::beast::http::field::content_length, std::to_string(body.length()));
                req.body() = body;
            }
            // This buffer is used for reading and must be persisted
            boost::beast::flat_buffer buffer;
            // Declare a container to hold the response
            // boost::beast::http::response<boost::beast::http::dynamic_body> res;
            boost::beast::http::response<boost::beast::http::string_body> res;
            if (s == "https") {
                // The SSL context is required, and holds certificates
                boost::asio::ssl::context ctx(boost::asio::ssl::context::tlsv12_client);
                // This holds the root certificate used for verification
                load_root_certificates(ctx);
                // Verify the remote server's certificate
                ctx.set_verify_mode(boost::asio::ssl::verify_peer);
                boost::beast::ssl_stream<boost::beast::tcp_stream> stream(ioc, ctx);
                //  Set SNI Hostname (many hosts need this to handshake successfully)
                if (!SSL_set_tlsext_host_name(stream.native_handle(), host.data())) {
                    boost::beast::error_code ec{static_cast<int>(::ERR_get_error()), boost::asio::error::get_ssl_category()};
                    // FIXME throw boost::beast::system_error{ec};
                }
                // Make the connection on the IP address we get from a lookup
                boost::beast::get_lowest_layer(stream).connect(results);
                // Perform the SSL handshake
                stream.handshake(boost::asio::ssl::stream_base::client);
                // Send the HTTP request to the remote host
                boost::beast::http::write(stream, req);
                // Receive the HTTP response
                boost::beast::http::read(stream, buffer, res);
                auto rh = res.base();
                auto rd = rh.result_int();
                try {
                    if (rd >= 300 && rd < 400) {
                        auto nu = rh.at("Location");
                        transfer(method, nu, headers, body, std::move(postProcessor));
                        // char buffer[256]{};
                        // sprintf(buffer, "%s", nu.data());
                        // printf("%s", buffer);
                    } else {
                        // postProcessor(boost::beast::buffers_to_string(res.body().data()));
                        postProcessor(std::move(res.body()));
                    }
                } catch (...) {}

                // Gracefully close the socket
                boost::beast::error_code ec;
                stream.shutdown(ec);
                if (ec == boost::beast::net::error::eof) {
                    // Rationale:
                    // http://stackoverflow.com/questions/25587403/boost-asio-ssl-async-shutdown-always-finishes-with-an-error
                    ec = {};
                }
            } else {
                boost::beast::tcp_stream stream(ioc);
                // Make the connection on the IP address we get from a lookup
                stream.connect(results);
                // Send the HTTP request to the remote host
                boost::beast::http::write(stream, req);
                // Receive the HTTP response
                boost::beast::http::read(stream, buffer, res);
                // postProcessor(boost::beast::buffers_to_string(res.body().data()));
                auto rh = res.base();
                auto rd = rh.result_int();
                try {
                    if (rd >= 300 && rd < 400) {
                        auto nu = rh.at("Location");
                        transfer(method, nu, headers, body, std::move(postProcessor));
                    } else {
                    postProcessor(std::move(res.body()));
                    }
                } catch (...) {}

                // Gracefully close the socket
                boost::beast::error_code ec;
                stream.socket().shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
                // not_connected happens sometimes
                // so don't bother reporting it.
                //
                if (ec && ec != boost::beast::errc::not_connected)
                    throw boost::beast::system_error{ec};

                // If we get here then the connection is closed gracefully
            }
        } catch (std::exception const& e) {
            // Log::log("Error: %s", e.what());
            postProcessor("");
        }
    }
    void transfer(std::string const& method, std::string const& url, std::map<std::string, std::string> const& headers, std::string const& body, std::string const& fileName,
        std::function<void(int&& r)>&& postProcessor) noexcept {
        try {
            boost::asio::io_context ioc;
            boost::asio::ip::tcp::resolver resolver(ioc);
            boost::urls::result<boost::urls::url_view> r = boost::urls::parse_uri(url);
            if (!r.has_value()) {
                postProcessor(1);
                return;
            }
            auto u = r.value();
            auto s = u.scheme();
            auto host = u.encoded_host();
            auto port = u.port();
            if (port == "") {
                port = s == "https" ? "https" : "http";
            }
            auto target = u.encoded_target();
            auto const results = resolver.resolve(host, port);
            constexpr int version = 11;
            boost::beast::http::verb httpMethod = boost::beast::http::verb::unknown;
            if (method == "GET") {
                httpMethod = boost::beast::http::verb::get;
            } else if (method == "POST") {
                httpMethod = boost::beast::http::verb::post;
            } else if (method == "PUT") {
                httpMethod = boost::beast::http::verb::put;
            } else if (method == "DELETE") {
                httpMethod = boost::beast::http::verb::delete_;
            } else if (method == "HEAD") {
                httpMethod = boost::beast::http::verb::head;
            } else if (method == "PATCH") {
                httpMethod = boost::beast::http::verb::patch;
            } else if (method == "PURGE") {
                httpMethod = boost::beast::http::verb::purge;
            } else if (method == "LINK") {
                httpMethod = boost::beast::http::verb::link;
            } else if (method == "UNLINK") {
                httpMethod = boost::beast::http::verb::unlink;
            } else if (method == "CONNECT") {
                httpMethod = boost::beast::http::verb::connect;
            } else if (method == "OPTIONS") {
                httpMethod = boost::beast::http::verb::options;
            } else if (method == "TRACE") {
                httpMethod = boost::beast::http::verb::trace;
            }
            boost::beast::http::request<boost::beast::http::string_body> req{httpMethod, target, version};
            for (auto const& header : headers) {
                req.insert(header.first, header.second);
            }
            req.set(boost::beast::http::field::host, host);
            req.set(boost::beast::http::field::user_agent, BOOST_BEAST_VERSION_STRING);
            if (body != "") {
                req.set(boost::beast::http::field::content_length, std::to_string(body.length()));
                req.body() = body;
            }
            boost::beast::flat_buffer buffer;
            boost::beast::error_code ec;
            boost::beast::http::response_parser<boost::beast::http::file_body> parser;
            parser.body_limit((std::numeric_limits<std::uint64_t>::max)());
            parser.get().body().open(fileName.c_str(), boost::beast::file_mode::write, ec);
            if (s == "https") {
                boost::asio::ssl::context ctx(boost::asio::ssl::context::tlsv12_client);
                load_root_certificates(ctx);
                ctx.set_verify_mode(boost::asio::ssl::verify_peer);
                boost::beast::ssl_stream<boost::beast::tcp_stream> stream(ioc, ctx);
                if (!SSL_set_tlsext_host_name(stream.native_handle(), std::string(host).c_str())) {
                    boost::beast::error_code ec{static_cast<int>(::ERR_get_error()), boost::asio::error::get_ssl_category()};
                }
                boost::beast::get_lowest_layer(stream).connect(results);
                stream.handshake(boost::asio::ssl::stream_base::client);
                boost::beast::http::write(stream, req);
                while (!parser.is_done()) {
                    boost::beast::http::read_some(stream, buffer, parser);
                }
                auto rh = parser.get().base();
                auto rd = rh.result_int();
                try {
                    parser.get().body().close();
                    if (rd >= 300 && rd < 400) {
                        auto nu = rh.at("Location");
                        transfer(method, nu, headers, body, fileName, std::move(postProcessor));
                    } else {
                        postProcessor(std::move(parser.get().result_int()));
                    }
                } catch (...) {}
                boost::beast::error_code ec;
                stream.shutdown(ec);
                if (ec == boost::beast::net::error::eof) {
                    ec = {};
                }
            } else {
                boost::beast::tcp_stream stream(ioc);
                stream.connect(results);
                boost::beast::http::write(stream, req);
                while (!parser.is_done()) {
                    boost::beast::http::read_some(stream, buffer, parser);
                }
                try {
                    parser.get().body().close();
                    postProcessor(std::move(parser.get().result_int()));
                } catch (...) {}
                boost::beast::error_code ec;
                stream.socket().shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
                if (ec && ec != boost::beast::errc::not_connected)
                    throw boost::beast::system_error{ec};
            }
        } catch (std::exception const& e) {
            printf("Error: %s", e.what());
            postProcessor(1);
        }
    }
} // namespace util::Http
