#include <gtest/gtest.h>
#include "http.h"
#include "headers.h"

TEST(IterHeaders, Empty)
{
    http::Head head("");
    
    EXPECT_TRUE(!head.method.size());
    EXPECT_TRUE(!head.uri.size());
    EXPECT_TRUE(!head.version.size());
    EXPECT_TRUE(!head.code.size());
    EXPECT_TRUE(!head.comment.size());
    EXPECT_TRUE(!head.headers.size());
}

TEST(IterHeaders, SkipRequestLine)
{
    http::Head head("Fake start line\r\n"
        "Header: value");
    
    EXPECT_TRUE(!head.method.size());
    EXPECT_TRUE(!head.uri.size());
    EXPECT_TRUE(!head.version.size());
    EXPECT_TRUE(!head.code.size());
    EXPECT_TRUE(!head.comment.size());
    EXPECT_TRUE(head.headers.size() == 2);
}

TEST(IterHeaders, StartingLine)
{
    using namespace std::string_view_literals;

    {
        http::Head head("GET http://uri.of.interest.html "
            "HTTP / 1.1 200 OK"sv);
        
        EXPECT_TRUE(!head.headers.size());
        EXPECT_TRUE(head.method == "GET"sv);
        EXPECT_TRUE(head.uri == "http://uri.of.interest.html"sv);
        EXPECT_TRUE(head.version == "1.1"sv);
        EXPECT_TRUE(head.code == "200"sv);
        EXPECT_TRUE(head.comment == "OK"sv);

        EXPECT_EQ(head.GetVersion(), 11);
        EXPECT_EQ(head.GetCode(), 200);
    }

    {
        http::Head head("GET HTTP / 1.1 200 OK"sv);
        
        EXPECT_TRUE(!head.headers.size());
        EXPECT_TRUE(head.method == "GET"sv);
        EXPECT_TRUE(!head.uri.size());
        EXPECT_TRUE(head.version == "1.1"sv);
        EXPECT_TRUE(head.code == "200"sv);
        EXPECT_TRUE(head.comment == "OK"sv);
    }

    {
        http::Head head("http://uri.of.interest.html HTTP / 1.1 200 OK"sv);
        
        EXPECT_TRUE(!head.headers.size());
        EXPECT_TRUE(head.method == "http://uri.of.interest.html"sv);
        EXPECT_TRUE(!head.uri.size());
        EXPECT_TRUE(head.version == "1.1"sv);
        EXPECT_TRUE(head.code == "200"sv);
        EXPECT_TRUE(head.comment == "OK"sv);
    }

    {
        http::Head head("GET http://uri.of.interest.html "
            "HTTP 1.1 200 OK"sv);
        
        EXPECT_TRUE(!head.headers.size());
        EXPECT_TRUE(head.method == "GET"sv);
        EXPECT_TRUE(head.uri == "http://uri.of.interest.html"sv);
        EXPECT_TRUE(!head.version.size());
        EXPECT_TRUE(!head.code.size());
        EXPECT_TRUE(!head.comment.size());
    }

    {
        http::Head head("GET http://uri.of.interest.html "
            "HTTP / 1.1 200 "sv);
        
        EXPECT_TRUE(!head.headers.size());
        EXPECT_TRUE(head.method == "GET"sv);
        EXPECT_TRUE(head.uri == "http://uri.of.interest.html"sv);
        EXPECT_TRUE(head.version == "1.1"sv);
        EXPECT_TRUE(head.code == "200"sv);
        EXPECT_TRUE(!head.comment.size());
    }

    {
        http::Head head("GET http://uri.of.interest.html "
            "HTTP / 2.0 OK "sv);
        
        EXPECT_TRUE(!head.headers.size());
        EXPECT_TRUE(head.method == "GET"sv);
        EXPECT_TRUE(head.uri == "http://uri.of.interest.html"sv);
        EXPECT_TRUE(head.version == "2.0"sv);
        EXPECT_TRUE(head.code == "OK"sv);
        EXPECT_TRUE(!head.comment.size());

        EXPECT_EQ(head.GetVersion(), 20);
        EXPECT_THROW(head.GetCode(), 
            std::invalid_argument);
    }

    {
        http::Head head("HTTP/2 308 Permanent Redirect  "sv);
        
        EXPECT_TRUE(!head.headers.size());
        EXPECT_TRUE(!head.method.size());
        EXPECT_TRUE(!head.uri.size());
        EXPECT_TRUE(head.version == "2"sv);
        EXPECT_TRUE(head.code == "308"sv);
        EXPECT_TRUE(head.comment == "Permanent Redirect"sv);

        EXPECT_EQ(head.GetVersion(), 20);
        EXPECT_EQ(head.GetCode(), 308);
    }
}

TEST(IterHeaders, SingleHeader)
{
    using namespace std::string_view_literals;

    http::Head head("Header   :   value"sv);
    
    EXPECT_TRUE(head.headers.size() == 1);

    if (head.headers.contains("Header"sv))
    {
        EXPECT_TRUE(head.headers.at("Header"sv) == 
            "value"sv);
    }
    else EXPECT_TRUE(head.headers.contains("Header"sv));
}

TEST(IterHeaders, MultipleHeaders)
{
    using namespace std::string_view_literals;

    http::Head head("Header_0   :   value_0\r\n"
        "   Header_1:value_1\r\n"
        "Header_2:    \r\n"
        "Header_3"sv);
    
    EXPECT_TRUE(head.headers.size() == 4);

    if (head.headers.contains("Header_0"sv))
    {
        EXPECT_TRUE(head.headers.at("Header_0"sv) == 
            "value_0"sv);
    }
    else EXPECT_TRUE(head.headers.contains("Header_0"sv));

    if (head.headers.contains("Header_1"sv))
    {
        EXPECT_TRUE(head.headers.at("Header_1"sv) == 
            "value_1"sv);
    }
    else EXPECT_TRUE(head.headers.contains("Header_1"sv));

    if (head.headers.contains("Header_2"sv))
    {
        EXPECT_TRUE(head.headers.at("Header_2"sv) == 
            ""sv);
    }
    else EXPECT_TRUE(head.headers.contains("Header_2"sv));

    if (head.headers.contains("Header_2"sv))
    {
        EXPECT_TRUE(head.headers.at("Header_3"sv) == 
            ""sv);
    }
    else EXPECT_TRUE(head.headers.contains("Header_3"sv));
}

TEST(IterHeaders, MultipleSameHeaders)
{
    // Последний непустой побеждает
    using namespace std::string_view_literals;

    {
        http::Head head("Header   :   value_0\r\n"
            "Header: value_1"sv);
        
        EXPECT_TRUE(head.headers.size() == 1);

        if (head.headers.contains("Header"sv))
        {
            EXPECT_TRUE(head.headers.at("Header"sv) == 
                "value_1"sv);
        }
        else EXPECT_TRUE(head.headers.contains("Header"sv));
    }

    {
        http::Head head("Header   :   value_0\r\n"
            "Header"sv);
        
        EXPECT_TRUE(head.headers.size() == 1);

        if (head.headers.contains("Header"sv))
        {
            EXPECT_TRUE(head.headers.at("Header"sv) == 
                "value_0"sv);
        }
        else EXPECT_TRUE(head.headers.contains("Header"sv));
    }
}

TEST(GetAddress, Baseline)
{
    using namespace std::string_view_literals;
    using Address = std::pair<std::string, short>;

    // Правильная обработка Host:    <name>  :  <port>
    {
        http::Head head("text text text\r\n"
            "Host:    1.2.3.4  :  100"sv);
        Address address = head.GetAddress();
        
        EXPECT_TRUE(address.first == "1.2.3.4");
        EXPECT_TRUE(address.second == 100);
    }

    // Правильная обработка Host:    <name>
    {
        http::Head head("text text text\r\n"
            "Host:    www.website.com"sv);
        Address address = head.GetAddress();
        
        EXPECT_TRUE(address.first == "www.website.com");
        EXPECT_TRUE(address.second == http::DEFAULT_PORT);
    }

    // Правильная обработка Host:<name>
    {
        http::Head head("text text text\r\n"
            "Host:www.website.com"sv);
        Address address = head.GetAddress();
        
        EXPECT_TRUE(address.first == "www.website.com");
        EXPECT_TRUE(address.second == http::DEFAULT_PORT);
    }

    // Правильная обработка Host:<name> при многостроковости
    {
        http::Head head("some text...\r\n"
            "Host: www.website.com\r\n"
            "more text..."sv);
        Address address = head.GetAddress();
        
        EXPECT_TRUE(address.first == "www.website.com");
        EXPECT_TRUE(address.second == http::DEFAULT_PORT);
    }

    // Неверный порт > 65536
    {
        http::Head head("text text text\r\n"
            "Host:    www.website.com:  1234567890"sv);
        Address address = head.GetAddress();
        
        EXPECT_TRUE(address.first == "www.website.com");

        // Порт <= 65535
        EXPECT_TRUE(address.second == http::DEFAULT_PORT);
    }

    // Запрет на '-' в начале и в конце
    {
        http::Head head("text text text\r\n"
            "Host: --wrong.website.com--"sv);
        Address address = head.GetAddress();
        
        EXPECT_TRUE(address.first.empty());
        EXPECT_TRUE(address.second == http::DEFAULT_PORT);
    }

    // Неверный формат Host = <name>
    {
        http::Head head("text text text\r\n"
            "Host = www.website.com"sv);
        Address address = head.GetAddress();
        
        EXPECT_TRUE(address.first.empty());
        EXPECT_TRUE(address.second == http::DEFAULT_PORT);
    }

    // Неверный формат Host <name>
    {
        http::Head head("text text text\r\n"
            "Host www.website.com"sv);
        Address address = head.GetAddress();
        
        EXPECT_TRUE(address.first.empty());
        EXPECT_TRUE(address.second == http::DEFAULT_PORT);
    }
}

TEST(GetAddress, Case)
{
    using namespace std::string_view_literals;
    using Address = std::pair<std::string, short>;

    // Правильная обработка HoSt: <name>:<port>
    {
        http::Head head("some text...\r\n"
            "HoSt: www.website.com:  100\r\n"
            "more text..."sv);
        Address address = head.GetAddress();
        
        EXPECT_TRUE(address.first == "www.website.com");
        EXPECT_TRUE(address.second == 100);
    }

    // Правильная обработка host: <name>:<port>
    {
        http::Head head("some text...\r\n"
            "host: www.website.com:  100\r\n"
            "more text..."sv);
        Address address = head.GetAddress();
        
        EXPECT_TRUE(address.first == "www.website.com");
        EXPECT_TRUE(address.second == 100);
    }

    // Правильная обработка HOST: <name>:<port>
    {
        http::Head head("some text...\r\n"
            "HOST: www.website.com:  100\r\n"
            "more text..."sv);
        Address address = head.GetAddress();
        
        EXPECT_TRUE(address.first == "www.website.com");
        EXPECT_TRUE(address.second == 100);
    }
}

TEST(GetAddress, NoHost)
{
    using namespace std::string_view_literals;
    using Address = std::pair<std::string, short>;

    // Отсутствие записи
    {
        http::Head head("text text text "
            "text text text"sv);
        Address address = head.GetAddress();
        
        EXPECT_TRUE(address.first.empty());
        EXPECT_TRUE(address.second == 80);
    }

    // Пустая запись
    {
        http::Head head("some text...\r\n"
            "Host: \r\n"
            "more text..."sv);
        Address address = head.GetAddress();
        
        EXPECT_TRUE(address.first.empty());
        EXPECT_TRUE(address.second == 80);
    }
}

TEST(GetBodySize, Baseline)
{
    using namespace std::string_view_literals;

    // Обычный случай
    {
        http::Head head("some text...\r\n"
            "Content-Length: 111"sv);
        std::optional<size_t> maybe_size = 
            head.GetBodySize();
        
        if (maybe_size) EXPECT_TRUE(*maybe_size == 111);
        else EXPECT_TRUE(maybe_size);
    }

    // Много пробелов
    {
        http::Head head("some text...\r\n"
            "Content-Length:    111"sv);
        std::optional<size_t> maybe_size = 
            head.GetBodySize();
        
        if (maybe_size) EXPECT_TRUE(*maybe_size == 111);
        else EXPECT_TRUE(maybe_size);
    }
}

TEST(GetBodySize, Case)
{
    using namespace std::string_view_literals;

    // Вперемешку
    {
        http::Head head("some text...\r\n"
            "ConTenT-LenGth: 1234"sv);
        std::optional<size_t> maybe_size = 
            head.GetBodySize();
        
        if (maybe_size) EXPECT_TRUE(*maybe_size == 1234);
        else EXPECT_TRUE(maybe_size);
    }

    // Только строчные
    {
        http::Head head("some text...\r\n"
            "content-length: 1234"sv);
        std::optional<size_t> maybe_size = 
            head.GetBodySize();
        
        if (maybe_size) EXPECT_TRUE(*maybe_size == 1234);
        else EXPECT_TRUE(maybe_size);
    }
}

TEST(GetBodySize, NoSize)
{
    using namespace std::string_view_literals;
    using Address = std::pair<std::string, std::string>;

    // Отсутствие записи
    {
        http::Head head("text text text "
            "text text text"sv);
        std::optional<size_t> maybe_size = 
            head.GetBodySize();
        
        EXPECT_FALSE(maybe_size);
    }

    // Пустая запись
    {
        http::Head head("some text...\r\n"
            "Content-Length: \r\n"
            "more text..."sv);
        std::optional<size_t> maybe_size = 
            head.GetBodySize();
        
        EXPECT_FALSE(maybe_size);
    }
}

TEST(HeaderEquals, Baseline)
{
    using namespace std::string_view_literals;

    // Обычный случай
    {
        http::Head head("Start string\r\n"
            "Header: value"sv);

        std::optional<bool> result = 
            head.HeaderEquals("HeAdEr"sv, 
                "vAlUe"sv);
        
        if (result) EXPECT_TRUE(*result);
        else EXPECT_TRUE(result);
    }

    // Много пробелов
    {
        http::Head head("Start string\r\n"
            "Header   :    value"sv);

        std::optional<bool> result = 
            head.HeaderEquals("HeAdEr"sv, 
                "vAlUe"sv);
        
        if (result) EXPECT_TRUE(*result);
        else EXPECT_TRUE(result);
    }
}