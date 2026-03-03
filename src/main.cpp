#include "http_relay.h"

#include <boost/asio.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <iostream>
#include <print>

#include <string_view>
#include <chrono>

#include <thread>

namespace server
{
    using namespace std::chrono;

    using namespace boost::asio;
    using namespace boost::asio::ip;
    using namespace boost::system;

    awaitable<void> session(tcp::socket client_socket, 
        io_context& io_ctx)
    {
        // Для общения с сервером
        tcp::socket server_socket(io_ctx);

        // Передатчики данных
        http::Relay client2server(client_socket, 
            server_socket);
        http::Relay server2client(server_socket, 
            client_socket);
        
        try
        {
            while (client2server.IsPersistent() && 
                server2client.IsPersistent())
            {
                co_await client2server.ReadHead();
                co_await client2server.RelayData();

                co_await server2client.ReadHead(false);
                co_await server2client.RelayData();
            }
        }
        catch(const system_error& err)
        {
            if (err.code() == error::connection_reset || 
                err.code() == error::eof) co_return;
            
            std::println(std::cerr, "Error: {}", 
                err.what());
        }
    }

    class Server
    {
    private:
        io_context& io_ctx_;
        tcp::acceptor acceptor_;

        awaitable<void> Listen()
        {
            while (true) try
            {
                tcp::socket socket = 
                    co_await acceptor_.async_accept(use_awaitable);
                
                co_spawn(io_ctx_, session(std::move(socket), io_ctx_), 
                    detached);
            }
            catch(const system_error& err)
            {
                if (err.code() == error::operation_aborted || 
                    err.code() == error::bad_descriptor) // 10009: the file handle is invalid
                {
                    std::println("The server is shutting down...");
                    co_return;
                }
                
                std::println(std::cerr, "Error: {}", 
                    err.what());
                throw;
            }
        }

    public:
        Server(io_context& io_ctx, short port) : 
            io_ctx_(io_ctx), 
            acceptor_(io_ctx_, tcp::endpoint(tcp::v4(), port))
        {}

        void Run()
        {
            co_spawn(io_ctx_, Listen(), detached);
            io_ctx_.run();
        }

        void Stop()
        {
            acceptor_.close();
        }
    };
}

void WaitToEnd(server::Server& server)
{
    using namespace std::string_literals;

    std::thread([&server]()
        {
            using namespace std::string_literals;

            std::string command;
            const std::string exit_command = "exit"s;
            bool exit_flag = false;

            // Цикл ожидания отмашки на завершение
            while (!exit_flag && 
                std::getline(std::cin, command))
            {
                if (command.size() != 
                    exit_command.size()) continue;
                
                size_t i = 0;
                for (i = 0; i < command.size(); ++i)
                {
                    if (std::tolower(command[i]) != 
                        std::tolower(exit_command[i])) break;
                }

                exit_flag = (i == command.size());
            }
            
            server.Stop();
        }).detach();
}

int main(int argc, char* argv[]) try
{
    using namespace server;

    if (argc != 2)
    {
        std::cerr << "Usage: AsyncHttpProxy ";
        std::cerr << "<listen_port>\n";
        return EXIT_FAILURE;
    }

    io_context io_ctx(1);
    server::Server server(io_ctx, std::atoi(argv[1]));
    
    WaitToEnd(server);
    server.Run();
}
catch (const std::exception& ex)
{
    std::cerr << "Exception: " << ex.what() << std::endl;
}