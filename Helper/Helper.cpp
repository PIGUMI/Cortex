#include "Helper.h"

std::string Helper::Utf8ToConsole(const std::string& utf8Str)
{
    if (utf8Str.empty()) return {};

    int wlen = MultiByteToWideChar(CP_UTF8, 0, utf8Str.c_str(), (int)utf8Str.size(), nullptr, 0);
    std::wstring wide(wlen, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8Str.c_str(), (int)utf8Str.size(), wide.data(), wlen);

    UINT cp = GetConsoleOutputCP(); // 実行中コンソールの出力コードページ(通常932)
    int alen = WideCharToMultiByte(cp, 0, wide.c_str(), wlen, nullptr, 0, nullptr, nullptr);
    std::string ansi(alen, '\0');
    WideCharToMultiByte(cp, 0, wide.c_str(), wlen, ansi.data(), alen, nullptr, nullptr);

    return ansi;
}

size_t Helper::Utf8SeqLen(unsigned char lead)
{
    if ((lead & 0x80) == 0x00) return 1;
    if ((lead & 0xE0) == 0xC0) return 2;
    if ((lead & 0xF0) == 0xE0) return 3;
    if ((lead & 0xF8) == 0xF0) return 4;
    return 1;
}

size_t Helper::InCompleteTailLen(const std::string& buf)
{
    for (size_t back = 1; back <= 4 && back <= buf.size(); back++)
    {
        unsigned char c = (unsigned char)buf[buf.size() - back];
        if ((c & 0xC0) != 0x80) // 継続バイトでない = 先頭バイト
        {
            size_t need = Utf8SeqLen(c);
            return (need > back) ? back : 0;
        }
    }
    return 0;
}

std::string Helper::StripThinkBlock(const std::string& text)
{
    const std::string open_tag = "<think>";
    const std::string close_tag = "</think>";

    size_t start = text.find(open_tag);
    if (start == std::string::npos) return text; // <think>が無ければそのまま返す

    size_t end = text.find(close_tag, start);
    if (end == std::string::npos) return text;

    return text.substr(0, start) + text.substr(end + close_tag.size());
}

