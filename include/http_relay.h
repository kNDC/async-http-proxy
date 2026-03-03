#pragma once

#include "http.h"
#include "headers.h"

#include <print>
#include <string>
#include <string_view>
#include <chrono>

#include <boost/asio.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>

namespace http
{
    using namespace boost::asio;
    using boost::asio::ip::tcp;
    using namespace boost::system;
    
    using namespace std::chrono;
    using namespace std::chrono_literals;

    class Relay
    {
    public:
        using Address = std::pair<std::string, unsigned>;

    private:
        constexpr static size_t BUFFER_SIZE = 8192;
        constexpr static seconds MAX_TIMEOUT = 10s;

        // Подлежащее хранилище данных
        std::string data_;

        // Буфер-надстройка
        dynamic_string_buffer<char, 
            std::char_traits<char>, 
            std::allocator<char>> buffer_;
        
        // Для отслеживания состояния буфера
        size_t n_bytes_read_ = 0;

        // Для хранения заглавия сообщения
        Head head_;

        // Слушаемый источник данных
        tcp::socket& src_;

        // Питаемый передатчик данных
        tcp::socket& dest_;
        Address dest_address_;

        // Даёт отмашку на закрытие соединения
        bool is_persistent_ = true;

        bool SkipOrNoBody(const http::Head& head) const;
        bool SetPersistence(const http::Head& head);

        template <typename AFn, 
            typename Out = typename std::invoke_result_t<AFn>::value_type, 
            typename std::enable_if_t<!std::is_same_v<Out, void>>* = nullptr>
        awaitable<Out> ExecuteOnTime(AFn afn);

        template <typename AFn, 
            typename Out = typename std::invoke_result_t<AFn>::value_type, 
            typename std::enable_if_t<std::is_same_v<Out, void>>* = nullptr>
        awaitable<Out> ExecuteOnTime(AFn afn);

        awaitable<void> ReadToDelimiter(std::string_view delimiter);
        
        awaitable<void> RelayHead();
        awaitable<void> RelayOneOff();
        awaitable<void> RelayWithSize(size_t msg_size);
        awaitable<void> RelayByChunks(bool has_trailer);

    public:
        Relay(tcp::socket& src, tcp::socket& dest) : 
            src_(src), dest_(dest),  
            buffer_(dynamic_buffer(data_, 
                BUFFER_SIZE))
        {
            data_.reserve(BUFFER_SIZE);
        }

        Relay(const Relay&) = delete;
        Relay& operator=(const Relay&) = delete;

        ~Relay() = default;
        
        awaitable<void> ReadHead(bool check_address = true);
        awaitable<void> RelayData();

        bool IsPersistent() const { return is_persistent_; }
    };
    
    template <typename AFn, 
        typename Out, 
        typename std::enable_if_t<!std::is_same_v<Out, void>>*>
    awaitable<Out> Relay::ExecuteOnTime(AFn afn)
    {
        using namespace boost::asio::experimental::awaitable_operators;

        // Отслеживатель времени
        steady_timer timeout(dest_.get_executor(), 
            MAX_TIMEOUT);
        
        std::variant<Out, std::monostate> result = 
            co_await(afn() || timeout.async_wait(use_awaitable));
        
        // Ожидание было слишком долгим
        if (result.index() == 1)
        {
            throw system_error(error::timed_out);
        }

        co_return std::get<0>(result);
    }

    template <typename AFn, 
        typename Out, 
        typename std::enable_if_t<std::is_same_v<Out, void>>*>
    awaitable<Out> Relay::ExecuteOnTime(AFn afn)
    {
        using namespace boost::asio::experimental::awaitable_operators;

        // Отслеживатель времени
        steady_timer timeout(dest_.get_executor(), 
            MAX_TIMEOUT);
        
        std::variant<std::monostate, std::monostate> result = 
            co_await(afn() || timeout.async_wait(use_awaitable));
        
        // Ожидание было слишком долгим
        if (result.index() == 1)
        {
            throw system_error(error::timed_out);
        }

        co_return;
    }
}  // namespace http