#include "http_relay.h"

namespace http
{
    bool Relay::SkipOrNoBody(const Head& head) const
    {
        using namespace std::string_view_literals;

        // Ответы без тела
        std::optional<unsigned> code = head.GetCode();

        if (code)
        {
            return *code < 200 || // Уведомляющие
                *code == 204 || // No Content
                *code == 205 || // Reset Content
                *code == 304; // Not Modified
        }

        if (std::optional<bool> result = 
            head.HeaderEquals("Transfer-Encoding:"sv, "chunks"s))
        {
            if (*result) return false;
        }
        
        return head.method == "GET" || 
            head.method == "HEAD" || 
            head.method == "DELETE" || 
            head.method == "OPTIONS" || 
            head.method == "TRACE";
    }

    awaitable<void> Relay::RelayHead() {
        co_await ExecuteOnTime([this]() -> awaitable<void>
            {
                co_await async_write(dest_, 
                    buffer(buffer_.data(), n_bytes_read_), 
                    use_awaitable);
            });
        
        buffer_.consume(n_bytes_read_);
    }

    bool Relay::SetPersistence(const http::Head& head)
    {
        using namespace std::string_view_literals;

        // Явное указание поддерживать соединение
        std::optional<bool> result = 
            head.HeaderEquals("Connection"sv, "keep-alive"sv);
        if (result && *result) return true;

        // Явное указание закрыть соединение
        result = head.HeaderEquals("Connection"sv, "close"sv);
        if (result && *result) return false;

        // HTTP/1.0 по умолчанию закрывается
        if (head.version == "1.0"sv) return false;
        return true;
    }

    awaitable<void> Relay::RelayOneOff() try
    {
        /* Отправка начала тела, если 
        оно было уже получено вместе с заголовком */
        if (buffer_.size())
        {
            co_await ExecuteOnTime([this]() -> awaitable<void>
                {
                    co_await async_write(dest_, 
                        buffer_, use_awaitable);
                });

            buffer_.consume(buffer_.size());
        }

        /* Хранилище данных для переноса данных 
        тела сообщения (строка может сбоить из-за 
        операций с выделением и освобождением памяти) */
        std::array<char, BUFFER_SIZE> write_buffer;

        while (true)
        {
            // Чтение
            n_bytes_read_ = co_await ExecuteOnTime([this, &write_buffer]() -> awaitable<size_t>
                {
                    co_return co_await src_.async_read_some(buffer(write_buffer, BUFFER_SIZE), 
                        use_awaitable);
                });
            
            // Запись
            co_await ExecuteOnTime([this, &write_buffer]() -> awaitable<void>
                {
                    co_await async_write(dest_, 
                        buffer(write_buffer, n_bytes_read_), 
                        use_awaitable);
                });
            
            buffer_.consume(n_bytes_read_);
        }
    }
    catch(const boost::system::system_error& err)
    {
        // Окончание передаваемого файла
        if (err.code() == boost::asio::error::eof) co_return;
        throw;
    }

    awaitable<void> Relay::RelayWithSize(size_t msg_size)
    {
        size_t n_bytes_left = msg_size;

        /* Отправка начала тела, если 
        оно уже было получено вместе с заголовком */
        if (buffer_.size())
        {
            size_t write_limit = std::min(n_bytes_left, buffer_.size());
            co_await ExecuteOnTime([this, write_limit]() -> awaitable<void>
                {
                    co_await async_write(dest_, 
                        buffer(buffer_.data(), write_limit), 
                        use_awaitable);
                });
            
            buffer_.consume(write_limit);
            n_bytes_left -= write_limit;
            //std::println("Leftover bytes consumed = {}", 
              //  write_limit);
        }

        if (!n_bytes_left) co_return;
        
        /* Хранилище данных для переноса данных 
        тела сообщения (строка может сбоить из-за 
        операций с выделением и освобождением памяти) */
        std::array<char, BUFFER_SIZE> write_buffer;
        
        /* Если отправка начала тела покрыла
        всё тело, то часть ниже пропустится. */
        while (n_bytes_left)
        {
            // Чтение
            size_t read_limit = std::min(BUFFER_SIZE, n_bytes_left);
            n_bytes_read_ = co_await ExecuteOnTime([this, &write_buffer, read_limit]() -> awaitable<size_t>
                {
                    co_return co_await src_.async_read_some(buffer(write_buffer, read_limit), 
                        use_awaitable);
                });
            
            // Запись
            size_t write_limit = std::min(n_bytes_read_, n_bytes_left);
            co_await ExecuteOnTime([this, &write_buffer, write_limit]() -> awaitable<void>
                {
                    co_await async_write(dest_, 
                        buffer(write_buffer, write_limit), 
                        use_awaitable);
                });

            n_bytes_left -= write_limit;
        }
    }

    awaitable<void> Relay::RelayByChunks(bool has_trailer)
    {
        unsigned chunk_size;
        unsigned i = 1;

        do
        {
            // Заголовок с длиной куска
            co_await ReadToDelimiter(CHUNK_DELIMITER);
            chunk_size = 0;
            
            // Определение длины куска
            {
                std::string_view head = 
                    { data_.data(), n_bytes_read_ };

                size_t pos = 0;
                while (pos < head.size() && 
                    std::isxdigit(head[pos]))
                {
                    chunk_size *= 16;
                    chunk_size += std::tolower(head[pos]) - 
                        (std::isdigit(head[pos++]) ? '0' : ('a' - 10));
                }
                
                // Кусок заканчивается \r\n
                chunk_size += CHUNK_DELIMITER.size();
            }

            // Отправка заголовка
            co_await RelayHead();

            // Отправка куска
            co_await RelayWithSize(chunk_size);
        } while (chunk_size > CHUNK_DELIMITER.size());

        // Отправка довеска к потоковому сообщению
        if (has_trailer)
        {
            co_await ReadHead(false);
            co_await RelayHead();
        }
    }

    awaitable<void> Relay::ReadToDelimiter(std::string_view delimiter)
    {
        // Чтение
        n_bytes_read_ = co_await ExecuteOnTime([this, delimiter]() -> awaitable<size_t>
            {
                co_return co_await async_read_until(src_, 
                    buffer_, delimiter, use_awaitable);
            });
    }

    awaitable<void> Relay::ReadHead(bool check_address)
    {
        co_await ReadToDelimiter(DELIMITER);

        // Разбор заголовка
        head_ = { { data_.data(), n_bytes_read_ } };

        if (head_.GetVersion() > 11)
        {
            system_error err(error::invalid_argument);
            err.code().message() = "HTTP versions above 1.1 are not supported (yet)";
            throw system_error(error::invalid_argument);
        }
        
        
        /* При передаче ответов от сервера 
        адресат уже известен и не указывается */
        if (!check_address) co_return;

        Address new_dest_address = head_.GetAddress();
        
        if (!dest_address_.first.size() && 
            !new_dest_address.first.size())
        {
            throw system_error(error::host_not_found);
        }
        
        if (dest_address_ != new_dest_address && 
            new_dest_address.first.size())
        {
            if (dest_.is_open()) dest_.close();

            dest_address_ = std::move(new_dest_address);
            
            tcp::resolver resolver(dest_.get_executor());
            ip::basic_resolver_results<tcp> endpoints = 
                co_await resolver.async_resolve(tcp::v4(), dest_address_.first, 
                    std::to_string(dest_address_.second), use_awaitable);
            co_await async_connect(dest_, endpoints, use_awaitable);
        }
    }

    awaitable<void> Relay::RelayData()
    {
        using namespace std::string_view_literals;
        
        // Случай поточной передачи
        bool is_chunked = 
            head_.HeaderExistsEquals("Transfer-Encoding"sv, 
                    "chunks"sv);

        // Длина сообщения
        std::optional<size_t> maybe_size = head_.GetBodySize();
        
        /* Поддерживать ли обмен данными после первой
        взаимной передачи? */
        is_persistent_ = SetPersistence(head_);

        // Есть ли у сообщения тело/нужно ли его пропускать?
        bool skip_or_no_body = SkipOrNoBody(head_);

        // Может присутствовать при поточной передаче
        bool has_trailer = head_.headers.contains("Trailer"sv);
        
        // Отправка заголовка
        co_await RelayHead();

        // Проверка на наличие тела сообщения
        if (skip_or_no_body) co_return;
    
        // Отправка тела сообщения
        // Случай потоковой передачи
        if (is_chunked)
        {
            co_await RelayByChunks(has_trailer);
            co_return;
        }
        
        // Случай включения ненулевой длины сообщения
        if (maybe_size && *maybe_size)
        {
            co_await RelayWithSize(*maybe_size);
            co_return;
        }

        // Случай отсутствия длины сообщения
        co_await RelayOneOff();
        co_return;
    }
} // namespace http