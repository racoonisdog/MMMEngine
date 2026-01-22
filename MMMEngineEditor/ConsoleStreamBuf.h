#pragma once
#include <streambuf>
#include <string>
#include <functional>

class ConsoleStreamBuf : public std::streambuf
{
public:
    using Callback = std::function<void(const std::string&)>;

    explicit ConsoleStreamBuf(Callback cb)
        : m_Callback(std::move(cb))
    {
    }

protected:
    int overflow(int ch) override
    {
        if (ch == traits_type::eof())
            return ch;

        char c = static_cast<char>(ch);
        if (c == '\n')
        {
            if (!m_Line.empty())
            {
                m_Callback(m_Line);
                m_Line.clear();
            }
        }
        else
        {
            m_Line.push_back(c);
        }
        return ch;
    }

    int sync() override
    {
        // flush될 때도 남은 내용 밀어넣기
        if (!m_Line.empty())
        {
            m_Callback(m_Line);
            m_Line.clear();
        }
        return 0;
    }

private:
    std::string m_Line;
    Callback m_Callback;
};
