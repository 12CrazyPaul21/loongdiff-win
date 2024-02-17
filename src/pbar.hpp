#ifndef __LDIFF_PBAR_H
#define __LDIFF_PBAR_H

import fmt;

#include <iostream>
#include <string>
#include <memory>
#include <tuple>
#include <type_traits>
#include <utility>
#include <thread>
#include <mutex>
#include <future>
#include <atomic>
#include <map>
#include <algorithm>

#include <Windows.h>

#include <traits.hpp>


namespace fp {

enum class OptionID
{
    delta = 0,
    vtstates,
    states,
    action,
    success_tips,
    failed_tips,
    success_color,
    failed_color,
    success_icon,
    failed_icon,
    fg_color,
    bar_width,
    left_mark,
    rigth_mark
};

template<typename T, OptionID Id, bool Required = false>
struct Option
{
    template<typename... Args, typename = typename std::enable_if<std::is_constructible<T, Args...>::value>::type>
    explicit Option(Args&&... args)
      : value(std::forward<Args>(args)...)
    {}

    Option(const Option&) = default;
    Option(Option&&)      = default;

    static constexpr auto id       = Id;
    static constexpr auto required = Required;
    using type                     = T;

    T value{};
};

using OptionDelta        = Option<std::uint32_t, OptionID::delta>;
using OptionVTStates     = Option<std::vector<std::u8string>, OptionID::vtstates>;
using OptionStates       = Option<std::vector<std::string>, OptionID::states>;
using OptionAction       = Option<std::string, OptionID::action, false>;
using OptionSuccessTips  = Option<std::string, OptionID::success_tips>;
using OptionFailedTips   = Option<std::string, OptionID::failed_tips>;
using OptionSuccessColor = Option<fmt::color, OptionID::success_color>;
using OptionFailedColor  = Option<fmt::color, OptionID::failed_color>;
using OptionSuccessIcon  = Option<std::u8string, OptionID::success_icon>;
using OptionFailedIcon   = Option<std::u8string, OptionID::failed_icon>;
using OptionFgColor      = Option<fmt::color, OptionID::fg_color>;
using OptionBarWidth     = Option<int, OptionID::bar_width>;
using OptionLeftMark     = Option<std::string, OptionID::left_mark>;
using OptionRightMark    = Option<std::string, OptionID::rigth_mark>;

class Spinner final
{
public:
    using Options =
        std::tuple<OptionDelta, OptionVTStates, OptionStates, OptionAction, OptionSuccessTips, OptionFailedTips,
                   OptionSuccessColor, OptionFailedColor, OptionSuccessIcon, OptionFailedIcon, OptionFgColor>;
    using RequiredOptions = traits::pitch_required<Options>::type;

    Spinner(const Spinner&) = delete;
    Spinner(Spinner&&)      = delete;

    template<typename... Args,
             typename std::enable_if<
                 traits::check_options<Options, RequiredOptions, typename std::decay<Args>::type...>::type::value,
                 void*>::type = nullptr>
    explicit Spinner(Args&&... args)
      : m_options(OptionDelta(40),
                  OptionVTStates(std::vector<std::u8string>{
                      u8"\u280B", u8"\u2819", u8"\u2839", u8"\u2838", u8"\u283C", u8"\u2834", u8"\u2826", u8"\u2827",
                      u8"\u2807", u8"\u280F"}),  // "⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"
                  OptionStates(std::vector<std::string>{"|", "/", "-", "\\"}), OptionAction("Loading"),
                  OptionSuccessTips("done"), OptionFailedTips("failed"), OptionSuccessColor(fmt::color::green),
                  OptionFailedColor(fmt::color::red), OptionSuccessIcon(u8"✔"), OptionFailedIcon(u8"✖"),
                  OptionFgColor(fmt::color::normal))
    {
        (set_option(std::forward<Args>(args)), ...);
    }

    template<typename T>
    inline void set_option(const T& t)
    {
        std::get<T>(m_options).value = t.value;
    }

    template<typename T>
    inline auto get_option() const
    {
        return std::get<T>(m_options).value;
    }

    inline bool is_running() const { return m_running; }

    void start(const std::string& action = "", const std::string& pre = "");
    void complete(bool f);

    inline void success() { complete(true); }
    inline void failed() { complete(false); }

private:
    void spin();
    void finish(bool f);

private:
    Options             m_options;
    std::mutex          m_mtx;
    bool                m_running{false};
    std::promise<bool>  m_signal;
    std::future<void>   m_exit;
    HANDLE              m_hstdout;
    CONSOLE_CURSOR_INFO m_cursorinfo;
    DWORD               m_climode{0};
    int                 m_indidx{0};
};

void Spinner::start(const std::string& action, const std::string& pre)
{
    std::lock_guard<std::mutex> lock(m_mtx);

    if (m_running) {
        return;
    }

    if (!action.empty()) {
        set_option(OptionAction(action));
    }

    m_indidx  = 0;
    m_running = true;
    m_signal  = std::promise<bool>();

    std::promise<void> exit_signal;
    m_exit = exit_signal.get_future();

    m_hstdout = GetStdHandle(STD_OUTPUT_HANDLE);
    GetConsoleCursorInfo(m_hstdout, &m_cursorinfo);

    CONSOLE_CURSOR_INFO info = m_cursorinfo;
    info.bVisible            = false;
    SetConsoleCursorInfo(m_hstdout, &info);

    GetConsoleMode(m_hstdout, &m_climode);

    if (!pre.empty()) {
        fmt::print(get_option<OptionFgColor>(), "{}\n", pre);
    }

    try {
        std::thread(
            [this](auto signal, auto future) {
                for (;;) {
                    if (future.wait_for(std::chrono::milliseconds(10)) != std::future_status::timeout) {
                        m_running = false;
                        finish(future.get());
                        signal.set_value();
                        return;
                    }

                    spin();
                }
            },
            std::move(exit_signal), std::move(m_signal.get_future()))
            .detach();
    }
    catch (...) {
    }
}

void Spinner::complete(bool f)
{
    std::lock_guard<std::mutex> lock(m_mtx);

    if (!m_running) {
        return;
    }

    try {
        m_signal.set_value(f);
        m_exit.wait();
    }
    catch (...) {
    }
}

void Spinner::spin()
{
    if (m_climode & ENABLE_VIRTUAL_TERMINAL_PROCESSING) {
        const auto& states = get_option<OptionVTStates>();
        if (m_indidx >= states.size()) {
            m_indidx = 0;
        }
        else {
            fmt::print_u8(states[m_indidx]);
            m_indidx = ++m_indidx % states.size();
        }
    }
    else {
        const auto& states = get_option<OptionStates>();
        if (m_indidx >= states.size()) {
            m_indidx = 0;
        }
        else {
            auto state = states[m_indidx];
            WriteConsoleA(m_hstdout, (const void*)(state.c_str()), (DWORD)state.size(), nullptr, nullptr);
            m_indidx = ++m_indidx % states.size();
        }
    }

    auto action = get_option<OptionAction>();
    if (!action.empty()) {
        fmt::print(get_option<OptionFgColor>(), "{}...", action);
    }

    WriteConsoleA(m_hstdout, (const void*)"\r", 1, nullptr, nullptr);

    std::this_thread::sleep_for(std::chrono::milliseconds(get_option<OptionDelta>()));
}

void Spinner::finish(bool f)
{
    fmt::erase_line();

    auto action = get_option<OptionAction>();

    std::string   tips;
    std::u8string icon;
    fmt::color    fg;

    if (f) {
        tips = get_option<OptionSuccessTips>();
        icon = get_option<OptionSuccessIcon>();
        fg   = get_option<OptionSuccessColor>();
    }
    else {
        tips = get_option<OptionFailedTips>();
        icon = get_option<OptionFailedIcon>();
        fg   = get_option<OptionFailedColor>();
    }

    if (!action.empty()) {
        if (m_climode & ENABLE_VIRTUAL_TERMINAL_PROCESSING) {
            fmt::print(" ");
            fmt::print_u8(icon);
        }

        fmt::print(get_option<OptionFgColor>(), " {} {}\n", action, fmt::color_str(fg, tips));
    }

    m_cursorinfo.bVisible = 1;
    SetConsoleCursorInfo(m_hstdout, &m_cursorinfo);
}

class BlockProgressPrinter
{
public:
    using Options =
        std::tuple<OptionBarWidth, OptionAction, OptionLeftMark, OptionRightMark, OptionSuccessTips, OptionFailedTips,
                   OptionSuccessColor, OptionFailedColor, OptionSuccessIcon, OptionFailedIcon>;
    using RequiredOptions = traits::pitch_required<Options>::type;

    BlockProgressPrinter(const BlockProgressPrinter&) = delete;
    BlockProgressPrinter(BlockProgressPrinter&&)      = delete;

    template<typename... Args,
             typename std::enable_if<
                 traits::check_options<Options, RequiredOptions, typename std::decay<Args>::type...>::type::value,
                 void*>::type = nullptr>
    explicit BlockProgressPrinter(Args&&... args)
      : m_options(OptionBarWidth(10), OptionAction("Loading"), OptionLeftMark("["), OptionRightMark("]"),
                  OptionSuccessTips("done"), OptionFailedTips("failed"), OptionSuccessColor(fmt::color::green),
                  OptionFailedColor(fmt::color::red), OptionSuccessIcon(u8"✔"), OptionFailedIcon(u8"✖"))
    {
        (set_option(std::forward<Args>(args)), ...);
    }

    template<typename T>
    inline void set_option(const T& t)
    {
        std::get<T>(m_options).value = t.value;
    }

    template<typename T>
    inline auto get_option() const
    {
        return std::get<T>(m_options).value;
    }

    inline bool is_running() const { return m_running; }

    void start(int nums, const std::string& action = "");
    void complete(bool f);
    void tick(const std::string& msg = "", bool wrap = true);

    inline void success() { complete(true); }
    inline void failed() { complete(false); }

private:
    void print_bar();

private:
    Options m_options;
    bool    m_running{false};
    int     m_idx{0};
    int     m_nums{0};
    DWORD   m_climode{0};
};

void BlockProgressPrinter::start(int nums, const std::string& action)
{
    if (m_running) {
        return;
    }
    m_running = true;

    m_idx  = 0;
    m_nums = nums;

    GetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), &m_climode);

    if (!action.empty()) {
        fmt::print(" Begin {}\n", action);
        set_option(OptionAction(action));
    }
}

void BlockProgressPrinter::complete(bool f)
{
    if (!m_running) {
        return;
    }
    m_running = false;

    m_idx  = 0;
    m_nums = 0;

    auto action = get_option<OptionAction>();

    std::string   tips;
    std::u8string icon;
    fmt::color    fg;

    if (f) {
        tips = get_option<OptionSuccessTips>();
        icon = get_option<OptionSuccessIcon>();
        fg   = get_option<OptionSuccessColor>();
    }
    else {
        tips = get_option<OptionFailedTips>();
        icon = get_option<OptionFailedIcon>();
        fg   = get_option<OptionFailedColor>();
    }

    if (!action.empty()) {
        if (m_climode & ENABLE_VIRTUAL_TERMINAL_PROCESSING) {
            fmt::print(" ");
            fmt::print_u8(icon);
        }

        fmt::print(" {} {}\n", action, fmt::color_str(fg, tips));
    }
}

void BlockProgressPrinter::tick(const std::string& msg, bool wrap)
{
    m_idx++;
    fmt::print("{}", get_option<OptionLeftMark>());
    print_bar();

    auto idx_str = std::to_string(m_idx);
    auto num_str = std::to_string(m_nums);
    if (num_str.size() > idx_str.size()) {
        fmt::print("{}", fmt::gen_space(num_str.size() - idx_str.size()));
    }

    fmt::print(" {}/{}{}", idx_str, num_str, get_option<OptionRightMark>());
    fmt::print(" {}{}", msg, wrap ? "\n" : "");
}

void BlockProgressPrinter::print_bar()
{
    static const std::vector<std::u8string> lead = {u8" ", u8"▏", u8"▎", u8"▍", u8"▌", u8"▋", u8"▊", u8"█"};

    int progress  = 0;
    int bar_width = (std::max)(get_option<OptionBarWidth>(), 10);

    if (m_idx > m_nums || m_nums == 0) {
        progress = (int)lead.size() * bar_width;
    }
    else {
        progress = (int)((double(m_idx) / double(m_nums)) * lead.size() * bar_width);
    }

    int done = progress / (int)lead.size();

    for (int i = 0; i < done; ++i) {
        fmt::print_u8(lead[lead.size() - 1]);
    }

    if (done == bar_width) {
        return;
    }

    fmt::print_u8(lead[progress % lead.size()]);

    for (int i = done + 1; i < bar_width; ++i) {
        fmt::print_u8(lead[0]);
    }
}

}  // namespace fp

#endif  // __LDIFF_PBAR_H