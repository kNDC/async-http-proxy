#include "http.h"
#include "headers.h"
#include "string_utils.h"

namespace http
{
    using namespace std::string_view_literals;

    const static decltype([](std::string_view sv, size_t pos = 0)
        {
            while (pos < sv.size() && 
                sv[pos] == ' ') ++pos;
            return pos;
        }) trim_front;
    
    const static decltype([](std::string_view sv, size_t pos)
        {
            while (pos && sv[pos - 1] == ' ') --pos;
            return pos;
        }) trim_back;

    void Head::ParseHead(std::string_view head)
    {
        std::vector<std::string_view> lines;

        // Разбиение на строки
        {
            size_t from = 0, to = 0;
            while (to < head.size())
            {
                switch (head[to])
                {
                case '\r':
                case '\n':
                    if (from != to)
                    {
                        lines.emplace_back(head.data() + from, 
                            to - from);
                    }
                    from = ++to;
                    break;
                default: ++to;
                }
            }

            if (from != to)
            {
                lines.emplace_back(head.data() + from, 
                    to - from);
            }
        }
        
        if (!lines.size()) return;
        
        // Ведущая строка
        std::string_view start_line = ""sv;
        size_t i = 0;
        if (FindLastNoCase(lines[0], "HTTP"sv) != 
            std::string_view::npos)
        {
            start_line = lines[0].substr(trim_front(lines[0]));
            start_line = start_line.substr(0, 
                trim_back(start_line, start_line.size()));
            
            ParseStartLine(start_line);
            i = 1;
        }

        // Строки с заголовками
        for (; i < lines.size(); ++i)
        {
            size_t sep_pos = 0;
            while (sep_pos < lines[i].size() && 
                lines[i][sep_pos] != ':') ++sep_pos;

            std::string_view header = lines[i].substr(0, sep_pos);
            sep_pos += (sep_pos < lines[i].size()) ? 1 : 0;

            std::string_view value = lines[i].substr(sep_pos, 
                lines[i].size() - sep_pos);

            header = header.substr(trim_front(header, 0));
            header = header.substr(0, 
                trim_back(header, header.size()));
            
            value = value.substr(trim_front(value));
            value = value.substr(0, 
                trim_back(value, value.size()));
            
            if (headers.contains(header) && 
                value.size()) headers[header] = value;
            else if (!headers.contains(header)) headers[header] = value;
        }
    }

    void Head::ParseStartLine(std::string_view start_line)
    {
        using namespace std::string_view_literals;

        size_t http_pos = 0;
        http_pos = FindLastNoCase(start_line, "HTTP"sv);
        if (http_pos == std::string::npos) return;


        decltype([](std::string_view sv, 
                std::pair<size_t, size_t> bounds)
            {
                bounds.first = bounds.second;

                bounds.first = trim_front(sv, bounds.first);
                bounds.second = bounds.first;

                while (bounds.second < sv.size() && 
                    sv[bounds.second] != ' ') ++bounds.second;
                return bounds;
            }) get_next_bounds;
        
        decltype([](std::string_view sv, 
                std::pair<size_t, size_t> bounds)
            {
                return std::string_view(sv.data() + bounds.first, 
                    bounds.second - bounds.first);
            }) bounds_to_word;
        
        // Левая половина с настройками запроса
        std::string_view left = start_line.substr(0, http_pos);
        left = left.substr(0, trim_back(left, left.size()));

        // Правая половина с настройками ответа
        std::string_view right = 
            start_line.substr(http_pos + "HTTP"sv.size());
        right = right.substr(trim_front(right));
        
        // Указание серверу
        std::pair<size_t, size_t> bounds{0, 0};
        bounds = get_next_bounds(left, bounds);
        method = bounds_to_word(left, bounds);
        
        // Запрашиваемый ресурс
        bounds = get_next_bounds(left, bounds);
        uri = bounds_to_word(left, bounds);
        
        // Версия HTTP: HTTP/*
        if (!right.size() || right[0] != '/') return;
        
        bounds = { 1, 1 };
        bounds = get_next_bounds(right, bounds);
        version = bounds_to_word(right, bounds);

        // Код
        bounds = get_next_bounds(right, bounds);
        code = bounds_to_word(right, bounds);

        // Пояснение
        bounds = get_next_bounds(right, bounds);
        comment = right.substr(bounds.first);
    }

    std::optional<bool> Head::HeaderEquals(std::string_view header, 
        std::string_view value) const noexcept
    {
        if (!headers.contains(header)) return std::nullopt;
        return CN_Equals<std::string_view>{}(headers.at(header), value);
    }

    bool Head::HeaderExistsEquals(std::string_view header, 
        std::string_view value)const noexcept
    {
        std::optional<bool> result = 
            HeaderEquals(header, value);
        return result && *result; 
    }

    std::optional<unsigned> Head::GetVersion() const
    {
        // version == ""sv => не указана
        if (!version.size()) return std::nullopt;
        
        unsigned out = 0;
        for (char c : version)
        {
            if (!(std::isdigit(c) || 
                c == '.')) return std::nullopt;
            if (c == '.') continue;
            
            out *= 10;
            out += c - '0';
        }
        
        out *= (version[0] != '0' && out < 10) ? 10 : 1;
        return out;
    }

    std::optional<unsigned> Head::GetCode() const
    {
        return (code.size())
            ? std::optional(std::stoul(std::string(code)))
            : std::nullopt;
    }

    std::optional<unsigned> Head::GetBodySize() const
    {
        using namespace std::string_view_literals;

        Headers::const_iterator it = 
            headers.find("Content-Length"sv);
        return (it != headers.end() && it->second.size())
            ? std::optional(std::stoull(std::string(it->second)))
            : std::nullopt;
    }

    std::pair<std::string, short> Head::GetAddress() const
    {
        using namespace std::string_view_literals;
        using Address = std::pair<std::string, short>;
        
        // Требуемый формат: Host: <host.com>:<port>
        std::pair<std::string, short> out{"", 
            http::DEFAULT_PORT};
        
        Headers::const_iterator it = headers.find("Host"sv);
        if (it == headers.end()) return out;
        
        std::string_view value = it->second;
        size_t pos = 0;

        // Вывод частей наименования получателя
        std::vector<std::string> labels;
        {
            std::string label;

            // Название получателя
            while (pos < value.size() && 
                (http::ValidateChar(value[pos]) || 
                value[pos] == '.'))
            {
                // Отсекаем приколы вроде 127.0..1...0.
                if (value[pos] == '.' && label.size())
                {
                    labels.emplace_back(std::move(label));
                }
                else label += value[pos];

                ++pos;
            }
            
            if (label.size()) labels.emplace_back(std::move(label));
            else if (labels.size()) labels.back() += '.';
        }
        
        // Проверка частей наименования получателя
        if (!http::ValidateHostLabels(labels)) return out;
        
        // Запись наименования
        {
            std::string_view sep = "";
            for (std::string& label : labels)
            {
                out.first += sep; sep = ".";
                out.first += std::move(label);
            }
        }

        // Чтение номера порта (если имеется)
        // Отсечение пробелов до двоеточия
        if ((pos = trim_front(value, pos)) == 
            value.size()) return out;

        // Если упёрлись не в двоеточие, то на выход
        if (value[pos++] != ':') return out;
        
        // Отсечение пробелов после двоеточия
        if ((pos = trim_front(value, pos)) == 
            value.size()) return out;

        unsigned port = 0;
        while (pos < value.size() && 
            std::isdigit(value[pos]))
        {
            port *= 10;
            port += value[pos++] - '0';
        }
        
        // Проверка порта
        if(http::ValidatePort(port))
        {
            out.second = port;
            return out;
        }
        
        return out;
    }
}