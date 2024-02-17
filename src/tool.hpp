#ifndef __LDIFF_TOOL_H
#define __LDIFF_TOOL_H

import file;
import fmt;

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iterator>
#include <array>
#include <vector>
#include <set>
#include <string>
#include <sstream>
#include <fstream>

#include <Windows.h>

#include <nlohmann/json.hpp>

#include <pbar.hpp>


static bool decode_vcdiff_u64_size(const uint8_t** ptr, uint64_t offset, const uint8_t* eptr, uint64_t* result)
{
    constexpr auto UINT64_OFLOW_MASK = 0xFE00000000000000;
    constexpr auto SIZEOF_USIZE_T    = 0x08;

    assert(ptr != nullptr && *ptr != nullptr);
    assert(eptr != nullptr);
    assert(result != nullptr);

    const uint8_t* p   = *ptr + offset;
    uint64_t       val = 0;

    *result = 0;

    for (;;) {
        if (p >= eptr || p - *ptr >= SIZEOF_USIZE_T) {
            return false;
        }

        if (val & UINT64_OFLOW_MASK) {
            return false;
        }

        val = (val << 7) | (*p & 0x7F);

        if ((*p & 0x80) == 0x0) {
            break;
        }

        p++;
    }

    *ptr    = p + 1;
    *result = val;

    return true;
}

namespace fp {

class FPTool
{
public:
    FPTool()  = default;
    ~FPTool() = default;

    bool setup(const std::string& path);
    bool execute(const std::vector<std::string>& args);

    std::string bin() const { return m_bin; }
    DWORD       last_exit_code() const { return m_lastExitCode; }

protected:
    std::string m_bin;
    DWORD       m_lastExitCode;
};

bool FPTool::setup(const std::string& path)
{
    m_bin = path;
    return true;
}

bool FPTool::execute(const std::vector<std::string>& args)
{
    std::stringstream   ss;
    STARTUPINFOA        si     = {0};
    PROCESS_INFORMATION pi     = {0};
    DWORD               dwFlag = CREATE_NEW_CONSOLE;

    ss << m_bin;
    std::for_each(args.begin(), args.end(), [&ss](const std::string& arg) { ss << " " << arg; });

    ZeroMemory(&si, sizeof(si));
    ZeroMemory(&pi, sizeof(pi));

    si.cb          = sizeof(si);
    si.dwFlags     = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    if (CreateProcessA(nullptr, const_cast<char*>(ss.str().c_str()), nullptr, nullptr, FALSE, dwFlag, nullptr, nullptr,
                       &si, &pi)) {
        WaitForSingleObject(pi.hProcess, INFINITE);
        GetExitCodeProcess(pi.hProcess, &m_lastExitCode);

        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);

        return true;
    }

    return false;
}

struct DiffMetaData
{
    int                      t;  // 0 -> file, 1 -> folder
    std::vector<std::string> p;  // patch
    std::vector<std::string> c;  // copy
    std::vector<std::string> d;  // delete

    bool serialize(const std::string& path);
    bool deserialize(const std::string& path);
};

bool DiffMetaData::serialize(const std::string& path)
{
    std::vector<std::string> _p;
    std::vector<std::string> _c;
    std::vector<std::string> _d;

    //
    // native to utf-8
    //

    // clang-format off
    std::transform(p.begin(), p.end(), std::back_inserter(_p), [](const std::string& s) {
        return fmt::native_to_utf8(s);
    });
    std::transform(c.begin(), c.end(), std::back_inserter(_c), [](const std::string& s) {
        return fmt::native_to_utf8(s);
    });
    std::transform(d.begin(), d.end(), std::back_inserter(_d), [](const std::string& s) {
        return fmt::native_to_utf8(s);
    });
    // clang-format on

    nlohmann::json j = {
        {"TYPE", t},
        {"PATCH", _p},
        {"COPY", _c},
        {"DELETE", _d},
    };

    std::string plaintext = j.dump(2);

    File file;
    if (!file.Create(path, plaintext.size(), true)) {
        return false;
    }

    return file.Write(file.Ptr(), (uint8_t*)plaintext.c_str(), plaintext.size());
}

bool DiffMetaData::deserialize(const std::string& path)
{
    std::ifstream stream(path);
    if (!stream.is_open()) {
        return false;
    }

    nlohmann::json j = nlohmann::json::parse(stream, nullptr, false);

    stream.close();

    if (j.is_discarded()) {
        return false;
    }

    //
    // utf-8 to native
    //

    bool retval = true;

    try {
        const std::vector<std::string>& _p = j["PATCH"];
        const std::vector<std::string>& _c = j["COPY"];
        const std::vector<std::string>& _d = j["DELETE"];

        // clang-format off
        std::transform(_p.begin(), _p.end(), std::back_inserter(p), [](const std::string& s) {
            return fmt::utf8_to_native(s);
        });
        std::transform(_c.begin(), _c.end(), std::back_inserter(c), [](const std::string& s) {
            return fmt::utf8_to_native(s);
        });
        std::transform(_d.begin(), _d.end(), std::back_inserter(d), [](const std::string& s) {
            return fmt::utf8_to_native(s);
        });
        // clang-format on

        t = j["TYPE"];
    }
    catch (...) {
        retval = false;
    }

    return retval;
}

enum class XdeltaDiffResult
{
    FAILED,
    SUCCESS,
    NEEDLESS
};

struct DiffSet
{
    std::vector<std::string> l;
    std::vector<std::string> r;
    std::vector<std::string> c;
};

class FPToolXdelta final : public FPTool
{
public:
    static constexpr const char* PATCH_EXT = ".lpatch";

    bool scan(const std::string& srcFolderPath, const std::string& tarFolderPath, DiffSet& dfFolders, DiffSet& dfFiles,
              const std::string& subFolder = "");
    XdeltaDiffResult diff(const std::string& source, const std::string& target, const std::string& patch);
    bool             patch(const std::string& source, const std::string& patch, const std::string& target);
    bool             verify_vcdiff(const std::string& path, bool fAutoDelete);

private:
    bool cmp_short_file(const std::string& source, const std::string& target, std::uintmax_t maxsize = 4);
    void diff_set(const std::set<std::string>& l, const std::set<std::string>& r, DiffSet& result);
};

bool FPToolXdelta::scan(const std::string& srcFolderPath, const std::string& tarFolderPath, DiffSet& dfFolders,
                        DiffSet& dfFiles, const std::string& subFolder)
{
    size_t ts = 0;

    {
        std::set<std::string> srcFolders;
        std::set<std::string> srcFiles;
        std::set<std::string> tarFolders;
        std::set<std::string> tarFiles;

        if (!File::ListFolder(srcFolderPath.c_str(), subFolder, srcFolders, srcFiles)) {
            return false;
        }

        if (!File::ListFolder(tarFolderPath.c_str(), subFolder, tarFolders, tarFiles)) {
            return false;
        }

        diff_set(srcFiles, tarFiles, dfFiles);

        ts = dfFolders.c.size();
        diff_set(srcFolders, tarFolders, dfFolders);
    }

    if (ts < dfFolders.c.size()) {
        for (size_t nums = dfFolders.c.size(); ts < nums; ts++) {
            scan(srcFolderPath, tarFolderPath, dfFolders, dfFiles, dfFolders.c[ts].c_str());
        }
    }

    return true;
}

XdeltaDiffResult FPToolXdelta::diff(const std::string& source, const std::string& target, const std::string& patch)
{
    std::uintmax_t srcb = File::GetFileSize(source);
    std::uintmax_t tarb = File::GetFileSize(target);

    if (srcb == tarb && srcb <= 4) {
        if (srcb == 0 || cmp_short_file(source, target, srcb)) {
            return XdeltaDiffResult::NEEDLESS;
        }
    }

    if (!File::MakeFilesFolder(patch)) {
        return XdeltaDiffResult::FAILED;
    }

    // clang-format off
    if (!execute({ "-f -s", fmt::gen("\"{}\"", source), fmt::gen("\"{}\"", target), fmt::gen("\"{}\"", patch) }) && m_lastExitCode == 0) {
        return XdeltaDiffResult::FAILED;
    }
    // clang-format on

    if (srcb == tarb && verify_vcdiff(patch, true)) {
        return XdeltaDiffResult::NEEDLESS;
    }

    return XdeltaDiffResult::SUCCESS;
}

bool FPToolXdelta::patch(const std::string& source, const std::string& patch, const std::string& target)
{
    if (!File::MakeFilesFolder(target)) {
        return false;
    }

    // clang-format off
    return execute({"-f -d -s", fmt::gen("\"{}\"", source), fmt::gen("\"{}\"", patch), fmt::gen("\"{}\"", target)}) && m_lastExitCode == 0;
    // clang-format on
}

/**
 * @see check_vcdiff_patch_identical.hexpat (ImHex Script)
 */
bool FPToolXdelta::verify_vcdiff(const std::string& path, bool fAutoDelete)
{
    static constexpr auto VCD_SOURCE = 1 << 0;
    static constexpr auto VCD_TARGET = 1 << 1;

    File           file;
    const uint8_t* ptr       = nullptr;
    const uint8_t* eptr      = nullptr;
    const uint8_t* nptr      = nullptr;
    uint64_t       size      = 0;
    uint8_t        indicator = 0;

    if (!file.Open(path, true)) {
        return false;
    }

    ptr  = file.ReadPtr();
    eptr = ptr + file.GetFileSize();

    // VCDIFF Magic
    if (*(uint32_t*)ptr != 0x00C4C3D6) {
        return false;
    }

    // length of application data
    if (!decode_vcdiff_u64_size(&ptr, 0x06, eptr, &size)) {
        return false;
    }

    // skip application data
    ptr += size;

    while (ptr < eptr) {
        // window indicator
        indicator = *ptr++;

        // not "COPY" window
        if (indicator & (VCD_SOURCE | VCD_TARGET)) {
            // length of copy window
            if (!decode_vcdiff_u64_size(&ptr, 0x0, eptr, &size)) {
                return false;
            }

            // offset of copy window
            if (!decode_vcdiff_u64_size(&ptr, 0x0, eptr, &size)) {
                return false;
            }
        }

        // length of the delta encoding
        if (!decode_vcdiff_u64_size(&ptr, 0x0, eptr, &size)) {
            return false;
        }

        // next window ptr
        nptr = ptr + size;

        // offset of target window
        if (!decode_vcdiff_u64_size(&ptr, 0x0, eptr, &size)) {
            return false;
        }

        // data section size
        if (!decode_vcdiff_u64_size(&ptr, 0x1, eptr, &size)) {
            return false;
        }

        if (size != 0) {
            return false;
        }

        ptr = nptr;
    }

    if (fAutoDelete) {
        file.Delete();
    }

    return true;
}

bool FPToolXdelta::cmp_short_file(const std::string& source, const std::string& target, std::uintmax_t maxsize)
{
    File f1, f2;

    if (!f1.Open(source.c_str(), true) || !f2.Open(target.c_str(), true)) {
        return false;
    }

    std::uintmax_t fs1 = f1.GetFileSize();
    std::uintmax_t fs2 = f2.GetFileSize();

    if (fs1 != fs2) {
        return false;
    }

    if (fs1 == 0) {
        return true;
    }

    std::uintmax_t sum = (std::min)(fs1, maxsize);
    const uint8_t* p1  = f1.ReadPtr();
    const uint8_t* p2  = f2.ReadPtr();

    for (std::uintmax_t i = 0; i < sum; i++) {
        if (*(p1 + i) != *(p2 + i)) {
            return false;
        }
    }

    return true;
}

void FPToolXdelta::diff_set(const std::set<std::string>& l, const std::set<std::string>& r, DiffSet& result)
{
    std::vector<std::string> difference;

    std::set_intersection(l.begin(), l.end(), r.begin(), r.end(), std::back_inserter(result.c));
    std::set_symmetric_difference(l.begin(), l.end(), r.begin(), r.end(), std::back_inserter(difference));

    for (auto const& i : difference) {
        if (r.find(i) != r.end()) {
            result.r.push_back(i);
        }
        else {
            result.l.push_back(i);
        }
    }
}

class FPTool7z final : public FPTool
{
public:
    bool zip(const std::string& folder, const std::string& target);
    bool unzip(const std::string& pack, const std::string& target);
};

bool FPTool7z::zip(const std::string& folder, const std::string& target)
{
    return execute({"a -r -t7z -mx=7", fmt::gen("\"{}\"", target), fmt::gen("\"{}\"", folder)}) && m_lastExitCode == 0;
}

bool FPTool7z::unzip(const std::string& pack, const std::string& target)
{
    return execute({"x -r -aoa", fmt::gen("\"{}\"", pack), fmt::gen("-o\"{}\"", target)}) && m_lastExitCode == 0;
}

class ToolBox final
{
private:
    ToolBox()
      : m_spinner(OptionDelta(40))
      , m_block_pbar(OptionBarWidth(10))
    {}

    ~ToolBox() = default;

public:
    static ToolBox& instance()
    {
        static ToolBox tb{};
        return tb;
    }

    bool setup(const std::string& appName);

    inline FPToolXdelta&         xdelta3() { return m_xdelta; }
    inline FPTool7z&             _7z() { return m_7z; }
    inline Spinner&              spinner() { return m_spinner; }
    inline BlockProgressPrinter& blockpbar() { return m_block_pbar; }

private:
    FPToolXdelta         m_xdelta;
    FPTool7z             m_7z;
    Spinner              m_spinner;
    BlockProgressPrinter m_block_pbar;
};

bool ToolBox::setup(const std::string& appName)
{
    // clang-format off
    static const auto TOOL_RESOURCES = std::array{
        std::array{"IDR_EXE_XDELTA3", "xdelta3.exe"},
        std::array{"IDR_EXE_7ZA", "7za.exe"},
        std::array{"IDR_DLL_7ZA", "7za.dll"}
    };
    // clang-format on

    auto appfolder = File::GetApplicationFolder(appName);
    if (appfolder.empty()) {
        return false;
    }

    for (auto const& res : TOOL_RESOURCES) {
        if (!File::ExportResource(appfolder.c_str(), res[1], res[0], "TOOL")) {
            return false;
        }
    }

    if (!m_xdelta.setup(File::CombinePath(appfolder.c_str(), "xdelta3.exe").c_str())) {
        return false;
    }

    if (!m_7z.setup(File::CombinePath(appfolder.c_str(), "7za.exe").c_str())) {
        return false;
    }

    return true;
}

inline auto& ToolBoxInstance() { return ToolBox::instance(); }
inline auto& XdeltaInstance() { return ToolBox::instance().xdelta3(); }
inline auto& _7zInstance() { return ToolBox::instance()._7z(); }
inline auto& SpinnerInstance() { return ToolBox::instance().spinner(); }
inline auto& BlockBarInstance() { return ToolBox::instance().blockpbar(); }

}  // namespace fp

#endif  // __LDIFF_TOOL_H