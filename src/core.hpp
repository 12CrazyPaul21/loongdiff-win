#ifndef __LDIFF_CORE_H
#define __LDIFF_CORE_H

import fmt;
import file;

#include <string>
#include <vector>
#include <set>
#include <tuple>
#include <algorithm>
#include <functional>
#include <map>
#include <csignal>

#include <Windows.h>

#include <config.h>
#include <pbar.hpp>
#include <tool.hpp>

using namespace fp;


struct PatchProgramArgs
{
    bool                ignore_backup;
    const std::string&  backup_path;
    const std::string&  src;
    const std::string&  workpath;
    const DiffMetaData& metadata;
};

struct RunMsg
{
    bool        err{false};
    fmt::color  color{fmt::color::normal};
    std::string msg;
};

using RunMsgMap = std::map<int, RunMsg>;

#define MAKE_RUNMSG(CODE, ERR, COLOR, MSG)                                                                             \
    {                                                                                                                  \
        CODE, RunMsg                                                                                                   \
        {                                                                                                              \
            ERR, COLOR, MSG                                                                                            \
        }                                                                                                              \
    }

#define SPINNER_RUN(ACTION, FUNC, ...)                                                                                 \
    do {                                                                                                               \
        int __spinner_ret = spinner_run(ACTION, FUNC, __VA_ARGS__);                                                    \
        if (__spinner_ret > 0) {                                                                                       \
            return __spinner_ret;                                                                                      \
        }                                                                                                              \
    } while (0)

#define PBAR_RUN(NUMS, ACTION, FUNC, ...)                                                                              \
    do {                                                                                                               \
        int __pbar_ret = pbar_run(NUMS, ACTION, FUNC, __VA_ARGS__);                                                    \
        if (__pbar_ret > 0) {                                                                                          \
            return __pbar_ret;                                                                                         \
        }                                                                                                              \
    } while (0)

inline void print_run_msg(int retval, const RunMsgMap& msgs)
{
    auto it = msgs.find(retval);
    if (it == msgs.end()) {
        return;
    }

    auto info = it->second;
    if (!info.err) {
        fmt::println(info.color, info.msg);
    }
    else {
        fmt::perrorln(info.color, info.msg);
    }
}

inline int spinner_run(const std::string& action, std::function<int()> func, const RunMsgMap& msgs = {})
{
    auto& spinner = SpinnerInstance();
    int   retval  = 0;

    spinner.start(action);

    if ((retval = func()) == 0) {
        spinner.success();
    }
    else {
        spinner.failed();
    }

    print_run_msg(retval, msgs);

    return retval;
}

inline int pbar_run(size_t nums, const std::string& action, std::function<int()> func, const RunMsgMap& msgs = {})
{
    auto& pbar   = BlockBarInstance();
    int   retval = 0;

    pbar.start((int)nums, action);

    if ((retval = func()) == 0) {
        pbar.success();
    }
    else {
        pbar.failed();
    }

    print_run_msg(retval, msgs);

    return retval;
}

template<typename... Args>
inline void pbar_tick(const std::string& format, Args&&... args)
{
    BlockBarInstance().tick("", false);
    fmt::print(format, args...);
}

inline fmt::color_str path_flag_colorful(const PathFlag& flag)
{
    switch (flag) {
        case PathFlag::NOT_EXISTS:
            return fmt::red_str("[not exists]");
        case PathFlag::FILE:
            return fmt::green_str("[file]");
        case PathFlag::FOLDER:
            return fmt::green_str("[folder]");
    }

    return fmt::color_str(fmt::color::normal, "");
}

inline std::string friendly_file_size(const std::string& path)
{
    static const std::array suffixes = {"B", "KB", "MB", "GB", "TB"};

    double  df    = (double)File::GetFileSize(path);
    uint8_t times = 0;

    while (df >= 1024. && times < suffixes.size() - 1) {
        times++;
        df /= 1024;
    }

    return fmt::gen("{}{}", df, suffixes[times]);
}

bool backup(const std::string& src, const PatchProgramArgs& args)
{
    if (args.ignore_backup) {
        return true;
    }

    std::string target = args.backup_path;
    if (target.empty()) {
        fmt::println_red("strongly recommend that you back up the original files!");
        if (!fmt::ask("do you need back up the original files?")) {
            return true;
        }
        target = fmt::request_file_path("please specify the backup file path", "ldiffbak.7z", [](std::string& path) {
            if (!File::IsPathExists(path.c_str())) {
                HANDLE hfile = CreateFileA(path.c_str(), GENERIC_ALL, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_HIDDEN, NULL);

                if (hfile != INVALID_HANDLE_VALUE) {
                    CloseHandle(hfile);
                    DeleteFileA(path.c_str());
                    path = File::GetFullPath(path.c_str());
                    return true;
                }
            }
            return false;
        });
    }

    if (!spinner_run("Backing up", [&]() { return !_7zInstance().zip(src, target); })) {
        fmt::println("generate backup -> {}[{}]", fmt::cyan_str(target), friendly_file_size(target));
        return true;
    }

    return false;
}

int diff_file(const std::string& src, const std::string& tar, const std::string& workpath)
{
    DiffMetaData metadata{0};
    auto&        xdelta = XdeltaInstance();

    std::string filename = File::GetFileName(src);
    std::string rootpath = File::CombinePath(workpath, "root");
    std::string patch    = File::CombinePath(rootpath, filename) + xdelta.PATCH_EXT;

    if (!File::Mkdir(rootpath)) {
        return 1;
    }

    // clang-format off
    fmt::println("{} {}[{}] and {}[{}] -> {}",
        fmt::green_str("diff"),
        fmt::cyan_str(src),
        friendly_file_size(src),
        fmt::cyan_str(tar),
        friendly_file_size(tar),
        patch
    );
    // clang-format on

    SPINNER_RUN(
        "Diffing",
        [&] {
            switch (xdelta.diff(src, tar, patch)) {
                case XdeltaDiffResult::FAILED:
                    return 1;
                case XdeltaDiffResult::NEEDLESS:
                    return 2;
                case XdeltaDiffResult::SUCCESS: {
                    metadata.p.push_back(filename);
                    break;
                }
            }
            return 0;
        },
        RunMsgMap{
            MAKE_RUNMSG(2, true, fmt::color::green,
                        "the source file and the target file are the same, so no patching is required!"),
        });

    // clang-format off
    return spinner_run("Serializing metadata", [&] {
        return !metadata.serialize(File::CombinePath(workpath, "metadata"));
    });
    // clang-format on
}

int diff_folder(const std::string& srcfolder, const std::string& tarfolder, const std::string& workpath)
{
    DiffMetaData metadata{1};
    DiffSet      dfolders;
    DiffSet      dfiles;
    auto&        xdelta = XdeltaInstance();

    // clang-format off
    SPINNER_RUN("Scanning files", [&] {
        return !xdelta.scan(srcfolder, tarfolder, dfolders, dfiles);
    });
    // clang-format on

    std::string rootpath = File::CombinePath(workpath, "root");
    if (!File::Mkdir(rootpath)) {
        fmt::perrorln_red("make root folder failed : {}", rootpath);
        return 1;
    }

    // diff common files
    PBAR_RUN(dfiles.c.size(), "Diff common files", [&]() {
        for (const std::string& filename : dfiles.c) {
            std::string src    = File::CombinePath(srcfolder, filename);
            std::string target = File::CombinePath(tarfolder, filename);
            std::string patch  = File::CombinePath(rootpath, filename) + xdelta.PATCH_EXT;

            // clang-format off
            pbar_tick("{} {}[{}] and {}[{}] -> {}",
                fmt::green_str("diff"),
                fmt::cyan_str(src),
                friendly_file_size(src),
                fmt::cyan_str(target),
                friendly_file_size(target),
                patch
            );
            // clang-format on

            switch (xdelta.diff(src, target, patch)) {
                case XdeltaDiffResult::FAILED:
                    fmt::perrorln_red(" failed");
                    return 1;
                case XdeltaDiffResult::NEEDLESS:
                    fmt::println_green(" same");
                    continue;
                case XdeltaDiffResult::SUCCESS: {
                    // the original file and target file are not the same, record patch
                    metadata.p.push_back(filename);
                    fmt::println_red(" different");
                    break;
                }
            }
        }

        return 0;
    });

    // delete all common empty folders
    SPINNER_RUN("Delete empty folder records", [&] {
        for (auto it = dfolders.c.rbegin(); it != dfolders.c.rend(); ++it) {
            auto folder = File::CombinePath(rootpath, *it);
            if (File::IsFolderEmpty(folder)) {
                File::Remove(folder);
            }
        }
        return 0;
    });

    // record: copy target only file
    PBAR_RUN(dfiles.r.size(), "Recording target only files", [&]() {
        for (const std::string& filename : dfiles.r) {
            std::string target = File::CombinePath(tarfolder, filename);
            std::string patch  = File::CombinePath(rootpath, filename);

            // clang-format off
            pbar_tick("{} {}[{}]",
                fmt::green_str("copy"),
                fmt::cyan_str(target),
                friendly_file_size(target)
            );
            // clang-format on

            if (!File::Copy(target, patch)) {
                fmt::perrorln_red(" failed");
                return 1;
            }

            metadata.c.push_back(filename);
            fmt::println_green(" success");
        }

        return 0;
    });

    // record: copy target only folder
    PBAR_RUN(dfolders.r.size(), "Recording target only folders", [&]() {
        for (const std::string& folder : dfolders.r) {
            std::string target = File::CombinePath(tarfolder, folder);
            std::string patch  = File::CombinePath(rootpath, folder);

            pbar_tick("{} {}", fmt::green_str("copy"), fmt::cyan_str(target));

            if (!File::Copy(target, patch)) {
                fmt::perrorln_red(" failed");
                return 1;
            }

            metadata.c.push_back(folder);
            fmt::println_green(" success");
        }

        return 0;
    });

    // record: files that need to be deleted
    PBAR_RUN(dfiles.l.size(), "Recording source only files", [&]() {
        for (const std::string& filename : dfiles.l) {
            pbar_tick("{} {}\n", fmt::cyan_str(filename), fmt::green_str("success"));
            metadata.d.push_back(filename);
        }

        return 0;
    });

    // record: folder that need to be deleted
    PBAR_RUN(dfolders.l.size(), "Recording source only folder", [&]() {
        for (const std::string& folder : dfolders.l) {
            pbar_tick("{} {}\n", fmt::cyan_str(folder), fmt::green_str("success"));
            metadata.d.push_back(folder);
        }

        return 0;
    });

    if (metadata.p.size() + metadata.c.size() + metadata.d.size() == 0) {
        fmt::perrorln_green("the source folder and the target folder are the same, so no patching is required!");
        return 2;
    }

    // clang-format off
    return spinner_run("Serializing metadata", [&] {
        return !metadata.serialize(File::CombinePath(workpath, "metadata"));
    });
    // clang-format on
}

int patch_file(const std::string& rootpath, const PatchProgramArgs& args)
{
    if (args.metadata.p.size() == 0) {
        fmt::perrorln_red("patch is empty");
        return 1;
    }

    auto& xdelta = XdeltaInstance();

    std::string filename  = args.metadata.p[0];
    std::string srcfile   = args.src;
    std::string tarfile   = File::CombinePath(rootpath, filename);
    std::string patchfile = tarfile + xdelta.PATCH_EXT;

    if (!File::IsFile(args.src)) {
        srcfile = File::CombinePath(args.src, filename);
        if (!File::IsPathExists(srcfile)) {
            fmt::perrorln_red("source file is not exist : {}", srcfile);
            return 1;
        }
    }

    if (!File::IsPathExists(patchfile)) {
        fmt::perrorln_red("patch file is not exist : {}", patchfile);
        return 1;
    }

    if (!backup(srcfile, args)) {
        return 1;
    }

    // clang-format off
    fmt::println("{} {} -> {}[{}]",
        fmt::green_str("patch"),
        fmt::cyan_str(patchfile),
        fmt::cyan_str(srcfile),
        friendly_file_size(srcfile)
    );
    // clang-format on

    // clang-format off
    SPINNER_RUN("Patching", [&] {
        return !xdelta.patch(srcfile, patchfile, tarfile);
    },
    RunMsgMap{
        MAKE_RUNMSG(1, true, fmt::color::red, fmt::gen("execute patch failed : {} -> {}", patchfile, srcfile)),
    });
    
    SPINNER_RUN("Applying", [&] {
        return !File::Copy(tarfile, srcfile);
    },
    RunMsgMap{
        MAKE_RUNMSG(1, true, fmt::color::red, fmt::gen("overwrite file failed : {} -> {}", tarfile, srcfile)),
    });
    // clang-format on

    return 0;
}

int patch_folder(const std::string& rootpath, const PatchProgramArgs& args)
{
    auto&                                            xdelta = XdeltaInstance();
    std::vector<std::pair<std::string, std::string>> vtpatched;

    if (File::IsFile(args.src)) {
        fmt::perrorln_red("for this patch, source must be a folder : {}", args.src);
        return 1;
    }

    if (!backup(args.src, args)) {
        return 1;
    }

    // do patch
    PBAR_RUN(args.metadata.p.size(), "Patching", [&]() {
        for (const auto& filename : args.metadata.p) {
            std::string srcfile   = File::CombinePath(args.src, filename);
            std::string tarfile   = File::CombinePath(rootpath, filename);
            std::string patchfile = tarfile + xdelta.PATCH_EXT;

            // clang-format off
            pbar_tick("{} {}[{}] -> {}[{}]",
                fmt::green_str("patch"),
                fmt::cyan_str(patchfile),
                friendly_file_size(patchfile),
                fmt::cyan_str(srcfile),
                friendly_file_size(srcfile)
            );
            // clang-format on

            if (!xdelta.patch(srcfile, patchfile, tarfile)) {
                fmt::perrorln_red(" failed");
                fmt::perrorln_red("execute patch failed : {} -> {}", patchfile, srcfile);
                return 1;
            }

            vtpatched.emplace_back(std::make_pair(std::move(tarfile), std::move(srcfile)));
            fmt::println_green(" success");
        }

        return 0;
    });

    // apply patch
    PBAR_RUN(vtpatched.size(), "Applying", [&]() {
        for (auto it = vtpatched.cbegin(); it != vtpatched.cend(); ++it) {
            // clang-format off
            pbar_tick("{} {}[{}] -> {}",
                fmt::green_str("apply"),
                fmt::cyan_str(it->first),
                friendly_file_size(it->first),
                fmt::cyan_str(it->second)
            );
            // clang-format on

            if (!File::Copy(it->first, it->second)) {
                fmt::perrorln_red(" failed");
                fmt::perrorln_red("overwrite file failed : {} -> {}", it->first, it->second);
                return 1;
            }
            fmt::println_green(" success");
        }

        return 0;
    });

    // delete
    PBAR_RUN(args.metadata.d.size(), "Remove source only", [&]() {
        for (auto it = args.metadata.d.rbegin(); it != args.metadata.d.rend(); ++it) {
            auto path = File::CombinePath(args.src, (*it));

            pbar_tick("{} {}\n", fmt::green_str("remove"), fmt::red_str(path));

            if (File::IsPathExists(path)) {
                File::Remove(path);
            }
        }

        return 0;
    });

    // copy
    PBAR_RUN(args.metadata.c.size(), "Copy target only", [&]() {
        for (const auto& sub : args.metadata.c) {
            std::string src    = File::CombinePath(rootpath, sub);
            std::string target = File::CombinePath(args.src, sub);

            // clang-format off
            pbar_tick("{} {}[{}] -> {}",
                fmt::green_str("copy"),
                fmt::cyan_str(src),
                friendly_file_size(src),
                fmt::cyan_str(target)
            );
            // clang-format on

            if (!File::Copy(src, target)) {
                fmt::perrorln_red(" failed");
                fmt::perrorln_red("copy file failed : {} -> {}", src, target);
                return 1;
            }

            fmt::println_green(" success");
        }

        return 0;
    });

    return 0;
}

int do_patch(const PatchProgramArgs& args)
{
    std::string rootpath = File::CombinePath(args.workpath, "root");
    if (!File::IsPathExists(rootpath)) {
        fmt::perrorln_red("patch is empty : {}", rootpath);
        return 1;
    }

    int retval = 0;

    switch (args.metadata.t) {
        case 0:
            retval = patch_file(rootpath, args);
            break;
        case 1:
            retval = patch_folder(rootpath, args);
            break;
        default:
            fmt::perrorln_red("invalid patch type : {}", args.metadata.t);
            return 1;
    }

    if (retval == 0) {
        fmt::println_green("apply patch completed!");
    }
    else {
        fmt::perrorln_red("apply patch incompleted!");
    }

    return retval;
}

#endif  // __LDIFF_CORE_H