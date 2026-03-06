#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    try {
        namespace asio = boost::asio;
        namespace http = boost::beast::http;

        asio::io_context io;
        asio::ip::tcp::resolver resolver{io};
        asio::ip::tcp::socket socket{io};

        auto const host = "127.0.0.1";
        auto const port = "8080";
        auto const target = std::format("/hello?name={}", argc ? argv[1] : "Client");

        auto results = resolver.resolve(host, port);
        asio::connect(socket, results);

        http::request<http::string_body> req{http::verb::get, target, 11};
        req.set(http::field::host, host);
        req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);

        http::write(socket, req);

        boost::beast::flat_buffer buffer;
        http::response<http::string_body> res;
        http::read(socket, buffer, res);

        std::cout << res.body() << std::endl;

        boost::system::error_code ec;
        socket.shutdown(asio::ip::tcp::socket::shutdown_both, ec);
        if (ec && ec != boost::system::errc::not_connected) {
            throw boost::system::system_error{ec};
        }
    } catch (const std::exception& ex) {
        std::cerr << "Request failed: " << ex.what() << std::endl;
        return 1;
    }

    return 0;
}
