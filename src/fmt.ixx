module;

#include <iostream>
#include <sstream>
#include <queue>
#include <functional>

#include <Windows.h>

export module fmt;

export namespace fp {
namespace fmt {

enum class color
{
    normal = 0,
    red,
    green,
    blue,
    cyan
};

struct color_str
{
    template<typename T>
    explicit color_str(color col, const T& arg)
      : c(col)
    {
        std::stringstream ss;

        if constexpr (std::is_same_v<std::decay_t<T>, bool>) {
            ss << std::boolalpha;
        }

        ss << arg;
        v = ss.str();
    }

    color_str(const color_str&) = default;
    color_str(color_str&&)      = default;

    color       c{};
    std::string v{};
};

}  // namespace fmt
}  // namespace fp

int switch_color(fp::fmt::color c)
{
    //
    // only modify the attributes of stdout
    // sync to stderr
    //

    static const char* vtcolors[] = {"", "31", "32", "34", "36"};
    static const WORD  ncolors[]  = {
        0, FOREGROUND_RED, FOREGROUND_GREEN, FOREGROUND_BLUE, FOREGROUND_BLUE | FOREGROUND_GREEN,
    };

    DWORD  mode    = 0;
    HANDLE hstdout = GetStdHandle(STD_OUTPUT_HANDLE);
    GetConsoleMode(hstdout, &mode);

    CONSOLE_SCREEN_BUFFER_INFO csbi{};
    if (!GetConsoleScreenBufferInfo(hstdout, &csbi)) {
        return -1;
    }

    if (mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING) {
        std::cout << std::string("\x1B[") + vtcolors[int(c)] + std::string("m");
    }
    else {
        // clang-format off
        SetConsoleTextAttribute(
            hstdout,
            (csbi.wAttributes & ~(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE)) | ncolors[int(c)]
        );
        // clang-format on    
    }

    return csbi.wAttributes;
}

void reset_color(int ctx)
{
    if (ctx >= 0) {
        SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), (WORD)ctx);
    }
}

template<typename T>
inline fp::fmt::color_str make_arg(const T& arg)
{
    return fp::fmt::color_str(fp::fmt::color::normal, arg);
}

template<>
inline fp::fmt::color_str make_arg(const fp::fmt::color_str& arg)
{
    return arg;
}

template<typename... Args>
std::queue<fp::fmt::color_str> make_vprint_args(std::tuple<Args...> const& args)
{
    // clang-format off
    std::queue<fp::fmt::color_str> qargs;
    std::apply([&](Args const&... tupleArgs) {
        ((qargs.emplace(std::move(make_arg(tupleArgs)))), ...);
    }, args);
    // clang-format on
    return std::move(qargs);
}

export namespace fp {
namespace fmt {

std::string to_string(const std::wstring& str, int codepage = CP_UTF8)
{
    int len = WideCharToMultiByte(codepage, 0, str.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (len == 0) {
        return "";
    }

    std::unique_ptr<char[]> buffer(new char[len]);
    if (buffer == nullptr) {
        return "";
    }

    WideCharToMultiByte(codepage, 0, str.c_str(), -1, buffer.get(), len, nullptr, nullptr);
    return std::string(buffer.get());
}

std::wstring to_wstring(const std::string& str, int codepage = CP_UTF8)
{
    int len = MultiByteToWideChar(codepage, 0, str.c_str(), -1, nullptr, 0);
    if (len == 0) {
        return L"";
    }

    std::unique_ptr<wchar_t[]> buffer(new wchar_t[len]);
    if (buffer == nullptr) {
        return L"";
    }

    MultiByteToWideChar(codepage, 0, str.c_str(), -1, buffer.get(), len);
    return std::wstring(buffer.get());
}

// clang-format off
inline std::string utf8_to_native(const std::string& str)
{
    return to_string(to_wstring(str, CP_UTF8), CP_ACP);
}

inline std::string native_to_utf8(const std::string& str)
{
    return to_string(to_wstring(str, CP_ACP), CP_UTF8);
}
//clang-format on

template<typename T>
color_str red_str(const T& arg)
{
    return color_str(color::red, arg);
}

template<typename T>
color_str green_str(const T& arg)
{
    return color_str(color::green, arg);
}

template<typename T>
color_str blue_str(const T& arg)
{
    return color_str(color::blue, arg);
}

template<typename T>
color_str cyan_str(const T& arg)
{
    return color_str(color::cyan, arg);
}

template<typename... Args>
void print_to_stream(std::ostream& os, const std::string& format, Args&&... args)
{
    int                   c = 0, n = 0;
    std::istringstream    iss(format);
    std::ostringstream    oss;
    std::queue<color_str> qargs = make_vprint_args(std::tuple<Args...>({args...}));

    while (!iss.eof()) {
        c = iss.get();
        if (c == -1) {
            break;
        }

        if (c == '{' && !qargs.empty()) {
            n = iss.get();
            if (n == '}') {
                auto arg = qargs.front();
                qargs.pop();

                if (arg.c == color::normal) {
                    oss << arg.v;
                }
                else {
                    os << oss.str();
                    oss.clear();
                    oss.str("");

                    int ctx = switch_color(arg.c);
                    os << arg.v;
                    reset_color(ctx);
                }
                continue;
            }

            iss.unget();
        }

        oss << (char)c;
    }

    os << oss.str();
}

template<typename... Args>
void print_to_stream(std::ostream& os, color c, const std::string& format, Args&&... args)
{
    if (c == color::normal) {
        print_to_stream(os, format, args...);
    }
    else {
        int ctx = switch_color(c);
        print_to_stream(os, format, args...);
        reset_color(ctx);
    }
}

template<typename... Args>
void print(const std::string& format, Args&&... args)
{
    print_to_stream(std::cout, format, args...);
}

template<typename... Args>
void print(color c, const std::string& format, Args&&... args)
{
    print_to_stream(std::cout, c, format, args...);
}

template<typename... Args>
void print_red(const std::string& format, Args&&... args)
{
    print_to_stream(std::cout, color::red, format, args...);
}

template<typename... Args>
void print_green(const std::string& format, Args&&... args)
{
    print_to_stream(std::cout, color::green, format, args...);
}

template<typename... Args>
void println(const std::string& format, Args&&... args)
{
    print_to_stream(std::cout, format, args...);
    std::cout << std::endl;
}

template<typename... Args>
void println(color c, const std::string& format, Args&&... args)
{
    print_to_stream(std::cout, c, format, args...);
    std::cout << std::endl;
}

template<typename... Args>
void println_red(const std::string& format, Args&&... args)
{
    println(color::red, format, args...);
}

template<typename... Args>
void println_green(const std::string& format, Args&&... args)
{
    println(color::green, format, args...);
}

template<typename... Args>
void perror(const std::string& format, Args&&... args)
{
    print_to_stream(std::cerr, format, args...);
}

template<typename... Args>
void perror(color c, const std::string& format, Args&&... args)
{
    print_to_stream(std::cerr, c, format, args...);
}

template<typename... Args>
void perror_red(const std::string& format, Args&&... args)
{
    print_to_stream(std::cerr, color::red, format, args...);
}

template<typename... Args>
void perror_green(const std::string& format, Args&&... args)
{
    print_to_stream(std::cerr, color::green, format, args...);
}

template<typename... Args>
void perrorln(const std::string& format, Args&&... args)
{
    print_to_stream(std::cerr, format, args...);
    std::cerr << std::endl;
}

template<typename... Args>
void perrorln(color c, const std::string& format, Args&&... args)
{
    print_to_stream(std::cerr, c, format, args...);
    std::cerr << std::endl;
}

template<typename... Args>
void perrorln_red(const std::string& format, Args&&... args)
{
    perrorln(color::red, format, args...);
}

template<typename... Args>
void perrorln_green(const std::string& format, Args&&... args)
{
    perrorln(color::green, format, args...);
}

template<typename... Args>
std::string gen(const std::string& format, Args&&... args)
{
    std::stringstream ss;
    print_to_stream(ss, format, args...);
    return ss.str();
}

std::string gen_space(size_t count)
{
    std::stringstream ss;

    for (size_t i = 0; i < count; i++) {
        ss << ' ';
    }

    return ss.str();
}

void print_u8(const std::u8string& u8)
{
    std::wstring u16 = to_wstring(reinterpret_cast<const char*>(u8.c_str()));
    WriteConsoleW(GetStdHandle(STD_OUTPUT_HANDLE), u16.data(), static_cast<DWORD>(u16.size()), nullptr, nullptr);
}

void erase_line()
{
    DWORD  mode = 0;
    HANDLE hstdout = GetStdHandle(STD_OUTPUT_HANDLE);
    
    GetConsoleMode(hstdout, &mode);

    if (mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING) {
        fputs("\x1B[1G\x1B[K", stdout);
    }
    else {
        DWORD                      written = 0;
        CONSOLE_SCREEN_BUFFER_INFO csbi{};

        if (!GetConsoleScreenBufferInfo(hstdout, &csbi)) {
            return;
        }

        csbi.dwCursorPosition.X = 0;

        FillConsoleOutputCharacterA(hstdout, ' ', csbi.dwSize.X, csbi.dwCursorPosition, &written);
        FillConsoleOutputAttribute(hstdout, csbi.wAttributes, csbi.dwSize.X, csbi.dwCursorPosition, &written);

        SetConsoleCursorPosition(hstdout, csbi.dwCursorPosition);
    }
}

bool ask(const std::string& msg)
{
    fmt::print("{} (y/n) : ", msg);

    do {
        char c;
        std::cin >> c;
        c = (char)std::tolower(c);

        std::cin.clear();
        std::cin.ignore((std::numeric_limits<std::streamsize>::max)(), '\n');

        switch (c) {
            case 'y':
                return true;
            case 'n':
                return false;
        }

        fmt::print("{} ({}) : ", msg, green_str("must y/n"));

    } while (true);

    return false;
}

std::string request_file_path(const std::string& msg, const std::string& ext, std::function<bool(std::string&)> func)
{
    std::string path;
    
    fmt::print("{} : ", msg);

    do {
        std::cin >> path;

        if (!ext.empty()) {
            path = path + "." + ext;
        }

        std::cin.clear();
        std::cin.ignore((std::numeric_limits<std::streamsize>::max)(), '\n');

        if (func == nullptr || func(path)) {
            break;
        }

        fmt::print("{} ({}) : ", msg, green_str("please specify a valid path"));

    } while (true);

    return path;
}

}  // namespace fmt
}  // namespace fp