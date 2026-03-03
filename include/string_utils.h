#pragma once

#include <vector>
#include <string_view>

template <typename Eq = std::equal_to<char>>
inline std::vector<size_t> PrefixFn(std::string_view sv, 
    Eq eq = {})
{
    std::vector<size_t> out(sv.size());
    out[0] = 0;
    size_t k = 0;

    for (size_t i = 1; i < sv.size(); ++i)
    {
        while (k && !eq(out[k], sv[i])) k = out[k - 1];
        k += eq(out[k], sv[i]);
        out[i] = k;
    }
    
    return out;
}

template <typename Eq = std::equal_to<char>>
inline size_t FindFirst(std::string_view text, 
    std::string_view pattern, size_t from = 0, 
    Eq eq = {})
{
    const std::vector<size_t> base = 
        PrefixFn(pattern, eq);
    size_t k = 0;

    for (size_t i = from; i < text.size(); ++i)
    {
        while (k && !eq(pattern[k], text[i])) k = base[k - 1];
        k += eq(pattern[k], text[i]);

        if (k == pattern.size()) return i - (k - 1);
    }
    
    return std::string_view::npos;
}

template <typename Eq = std::equal_to<char>>
inline size_t FindLast(std::string_view text, 
    std::string_view pattern, Eq eq = {})
{
    return FindLast(text, pattern, 
        pattern.size(), eq);
}

inline size_t FindFirstNoCase(std::string_view text, 
    std::string_view pattern, size_t from = 0)
{
    return FindFirst(text, pattern, from, 
        [](char c1, char c2)
        {
            return std::tolower(c1) == std::tolower(c2);
        });
}

template <typename Eq = std::equal_to<char>>
inline std::vector<size_t> FindAll(std::string_view text, 
    std::string_view pattern, size_t from = 0, 
    Eq eq = {})
{
    std::vector<size_t> out; out.reserve(text.size());
    std::vector<size_t> base = PrefixFn(pattern, eq);
    size_t k = 0;

    for (size_t i = from; i < text.size(); ++i)
    {
        while (k && !eq(pattern[k], text[i])) k = base[k - 1];
        k += eq(pattern[k], text[i]);

        if (k == pattern.size())
        {
            out.emplace_back(i - (k - 1));
            k = base[k - 1];
            i -= k;
        }
    }
    
    return out;
}

inline std::vector<size_t> FindAllNoCase(std::string_view text, 
    std::string_view pattern, size_t from = 0)
{
    return FindAll(text, pattern, from, 
        [](char c1, char c2)
        {
            return std::tolower(c1) == std::tolower(c2);
        });
}

template <typename Eq = std::equal_to<char>>
inline size_t FindLast(std::string_view text, 
    std::string_view pattern, size_t from = 0, 
    Eq eq = {})
{
    std::vector v = FindAll(text, pattern, from, eq);
    return (v.size()) ? v.back() : std::string_view::npos;
}

inline size_t FindLastNoCase(std::string_view text, 
    std::string_view pattern, size_t from = 0)
{
    std::vector v = FindAll(text, pattern, from, 
        [](char c1, char c2)
        {
            return std::tolower(c1) == std::tolower(c2);
        });
    return (v.size()) ? v.back() : std::string_view::npos;
}