#include "tokenizer.hpp"
#include <cctype>

namespace nlp {
    std::string normalizeRussian (const std::string& text) {
        static const std::pair <const char*, const char*> p[] = {
        {"А","а"},{"Б","б"},{"В","в"},{"Г","г"},{"Д","д"},{"Е","е"},{"Ё","ё"},{"Ж","ж"},{"З","з"},{"И","и"},{"Й","й"},{"К","к"},
        {"Л","л"},{"М","м"},{"Н","н"},{"О","о"},{"П","п"},{"Р","р"},{"С","с"},{"Т","т"},{"У","у"},{"Ф","ф"},{"Х","х"},{"Ц","ц"},
        {"Ч","ч"},{"Ш","ш"},{"Щ","щ"},{"Ъ","ъ"},{"Ы","ы"},{"Ь","ь"},{"Э","э"},{"Ю","ю"},{"Я","я"}
        };
        std::string s = text;
        for (auto& x: p) {
            std::size_t pos = 0;
            while ((pos=s.find(x.first,pos)) !=std::string::npos) {
                s.replace (pos, 2, x.second);
                pos += 2;
            }
        }
        std::size_t pos = 0;
        while ((pos = s.find("ё",pos)) != std::string::npos) {
            s.replace (pos, 2, "е");
        }
        return s;
    }
    std::vector <std::string> tokenize (const std::string& text) {
        std::string s = normalizeRussian (text);
        std::vector <std::string> out;
        std::string w;
        for (unsigned char c:s) {
            if (std::isspace(c) || c == ',' || c == '.' || c == '?' || c == '!' || c == ';'||c==':' || c == '(' || c == ')') {
                if (!w.empty()) {
                    out.push_back(w);
                    w.clear();
                }
            }
            else w.push_back((char)c);
        }
        if (!w.empty()) out.push_back(w);
        return out;
    }
}
