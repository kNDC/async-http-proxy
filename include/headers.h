#pragma once

#include <string>
#include <string_view>
#include <cwctype>

#include <unordered_map>

#include <optional>

namespace http
{
    struct Head
    {
    private:
        template <typename SV>
        struct CN_Hasher
        {
            // Безрегистровое кодирование строк
            size_t operator()(SV sv) const
            {
                size_t hash = 5381;

                for (size_t i = 0; i < sv.size(); ++i)
                {
                    char c = std::towlower(sv[i]);
                    hash = hash * 33 ^ c;
                }
                
                return hash;
            }
        };

        // Безрегистровое сравнение строк
        template <typename SV>
        struct CN_Equals
        {
        private:
            bool CompareEqualLength(SV sv1, SV sv2) const
            {
                for (size_t i = 0; i < sv1.size(); ++i)
                {
                    if (sv1[i] == sv2[i]) continue;
                    if (std::towlower(sv1[i]) == 
                        std::towlower(sv2[i])) continue;
                    return false;
                }
                
                return true;
            }

        public:
            bool operator()(SV sv1, SV sv2) const
            {
                return sv1.size() == sv2.size() && 
                    CompareEqualLength(sv1, sv2);
            }
        };

        void ParseHead(std::string_view);
        void ParseStartLine(std::string_view);

    public:
        using Headers = 
            std::unordered_map<std::string_view, 
                std::string_view, 
                CN_Hasher<std::string_view>, 
                CN_Equals<std::string_view>>;
        
        Head() = default;

        Head(std::string_view head)
        {
            ParseHead(head);
        }

        ~Head() = default;

        std::string_view method;
        std::string_view uri;
        std::string_view version;
        std::string_view code;
        std::string_view comment;

        Headers headers;
        
        std::optional<bool> HeaderEquals(std::string_view header, 
            std::string_view value) const noexcept;
        bool HeaderExistsEquals(std::string_view header, 
            std::string_view value) const noexcept;
        
        std::optional<unsigned> GetVersion() const;
        std::optional<unsigned> GetCode() const;
        std::optional<unsigned> GetBodySize() const;

        std::pair<std::string, short> GetAddress() const;
    };
}