#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace http
{
    using namespace std::string_literals;
    
    inline bool ValidateChar(char c)
    {
        if (std::isalpha(c)) return true;
        if (std::isdigit(c)) return true;
        if (c == '-') return true;
        if (c == '_') return true;
        
        return false;
    }

    inline bool ValidateHostLabels(const std::vector<std::string>& labels)
    {
        if (!labels.size()) return false;
        
        unsigned total_size = 0;
        for (const std::string& label: labels)
        {
            if (!label.size()) return false;
            if (label.size() > 63) return false;

            // Включение разделяющей точки
            total_size += label.size() + 1;
        }

        // Не учитываем (возможную) завершающую точку
        --total_size;
        
        return !(total_size > 253 || 
            labels.front().front() == '-' || 
            labels.back().back() == '-');
    }

    inline bool ValidatePort(unsigned val)
    {
        return val <= (unsigned short)-1;
    }

    enum class Version : char
    {
        v10,
        v11,
        Unsupported
    };

    // Разделитель заголовка и тела сообщения
    constexpr static std::string DELIMITER = "\r\n\r\n"s;

    // Разделитель заголовка и тела сообщения
    constexpr static std::string CHUNK_DELIMITER = "\r\n"s;

    // 80 - порт по умолчанию в http (443 в https)
    constexpr static short DEFAULT_PORT = 80;
} // namespace http