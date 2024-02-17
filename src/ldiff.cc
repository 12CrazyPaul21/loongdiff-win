/**
 * @file ldiff.cc
 * @author phantom (60491683@qq.com)
 * @version 0.1
 * 
 * loong diff(ldiff)
 * diff & patch
 */

#include <CLI/CLI.hpp>

#include <core.hpp>


static std::string g_tempfolder{""};
static std::string g_workfolder{""};
static bool        g_ctrlc = false;
static bool        g_keeptemp = false;


int diff_program(CLI::App* sub_diff, PatchPE& pe)
{
    int         retval = 0;
    std::string patname, tmppat, tmpdest, output;

    std::string src = File::GetFullPath(sub_diff->get_option("source")->as<std::string>());
    std::string tar = File::GetFullPath(sub_diff->get_option("target")->as<std::string>());
    std::string pat = File::GetFullPath(sub_diff->get_option("patch")->as<std::string>());

    bool     forceflag = sub_diff->get_option("-f")->as<bool>();
    PathFlag srcflag   = File::CheckPathFlag(src);
    PathFlag tarflag   = File::CheckPathFlag(tar);
    PathFlag patflag   = File::CheckPathFlag(pat + FPToolXdelta::PATCH_EXT);
    PathFlag patpeflag = File::CheckPathFlag(pat + ".exe");

    fmt::println("source path : {} {}", src, path_flag_colorful(srcflag));
    fmt::println("target path : {} {}", tar, path_flag_colorful(tarflag));
    fmt::println("patch path : {}", pat);

    if (!forceflag && (patflag != PathFlag::NOT_EXISTS || patpeflag != PathFlag::NOT_EXISTS)) {
        fmt::perrorln_red("patch file with the same name it's already exists");
        return 1;
    }

    if (srcflag == PathFlag::NOT_EXISTS || tarflag == PathFlag::NOT_EXISTS) {
        fmt::perrorln_red("source or target not exists");
        return 1;
    }

    if (srcflag != tarflag) {
        if (srcflag == PathFlag::FILE) {
            tar = File::CombinePath(tar, File::GetFileName(src));
            if (!File::IsFile(tar)) {
                fmt::perrorln_red("target not exists or not a file : {}", tar);
                return 1;
            }
            tarflag = PathFlag::FILE;
        }
        else {
            src = File::CombinePath(src, File::GetFileName(tar));
            if (!File::IsFile(src)) {
                fmt::perrorln_red("source not exists or not a file : {}", tar);
                return 1;
            }
            srcflag = PathFlag::FILE;
        }
    }

    g_tempfolder = File::CombinePath(File::GetParentPath(pat), APP_TEMP);
    g_workfolder = File::CombinePath(g_tempfolder, "work");
    g_keeptemp   = sub_diff->get_option("-k")->as<bool>();

    fmt::println("temp folder : {}", g_tempfolder);
    fmt::println("work folder : {}", g_workfolder);

    if (File::IsPathExists(g_tempfolder)) {
        File::Remove(g_tempfolder);
    }
    if (!File::Mkdir(g_tempfolder)) {
        fmt::perrorln_red("make temp folder failed : {}", g_tempfolder);
        return 1;
    }
    if (!File::Mkdir(g_workfolder)) {
        fmt::perrorln_red("make work folder failed : {}", g_workfolder);
        return 1;
    }

    File::HidePath(g_tempfolder);

    //
    // execute diff
    //

    if (srcflag == PathFlag::FILE) {
        retval = diff_file(src, tar, g_workfolder);
    }
    else {
        retval = diff_folder(src, tar, g_workfolder);
    }

    if (retval) {
        return retval;
    }

    //
    // package patch
    //

    patname = File::GetFileName(pat);
    tmppat  = File::CombinePath(g_tempfolder, patname + FPToolXdelta::PATCH_EXT);

    // clang-format off
    SPINNER_RUN("Packing patch", [&] {
        return !ToolBoxInstance()._7z().zip(g_workfolder, tmppat);
    },
    RunMsgMap{
        MAKE_RUNMSG(1, true, fmt::color::red, fmt::gen("package patch failed : {}", tmppat)),
    });
    // clang-format on

    //
    // output
    //

    tmpdest = tmppat;
    output  = pat;

    if (sub_diff->get_option("-e")->as<bool>()) {
        //
        // generate an executable patch file
        //

        tmpdest = File::CombinePath(g_tempfolder, patname + ".exe");

        // clang-format off
        SPINNER_RUN("Generating exe", [&] {
            if (!File::Copy(pe.GetFullPath(), tmpdest)) {
                return 1;
            }

            PatchPE f(tmpdest, false);
            if (!f.IsOpened()) {
                return 2;
            }

            if (f.GetFileSize() + File::GetFileSize(tmppat) >= 0xF3333333) {
                tmpdest = tmppat;
                output += FPToolXdelta::PATCH_EXT;
                return -1;
            }
            else {
                output += ".exe";

                if (!f.AppendPatch(tmppat)) {
                    return 3;
                }
            }

            return 0;
        },
        RunMsgMap{
            MAKE_RUNMSG(-1, true, fmt::color::green, "pe file + patch is over 3.8GB, can't output to an executable file"),
            MAKE_RUNMSG(1, true, fmt::color::red, fmt::gen("copy original application exe failed : {} => {}", pe.GetFullPath(), tmpdest)),
            MAKE_RUNMSG(2, true, fmt::color::red, fmt::gen("open pe file failed : {}", tmpdest)),
            MAKE_RUNMSG(3, true, fmt::color::red, "append patch to pe file failed"),
        });
        // clang-format on
    }
    else {
        // just patch
        output += FPToolXdelta::PATCH_EXT;
    }

    // clang-format off
    SPINNER_RUN("Release patch", [&] {
        return !File::Copy(tmpdest, output);
    },
    RunMsgMap{
        MAKE_RUNMSG(1, true, fmt::color::red, fmt::gen("release patch output failed : {} => {}", tmpdest, output)),
    });
    // clang-format on

    fmt::println("generate patch to : {}[{}]", fmt::green_str(output), friendly_file_size(output));

    return 0;
}

int patch_program(CLI::App* sub_patch)
{
    DiffMetaData metadata;

    std::string src = File::GetFullPath(sub_patch->get_option("source")->as<std::string>());
    std::string pat = File::GetFullPath(sub_patch->get_option("patch")->as<std::string>());

    PathFlag srcflag = File::CheckPathFlag(src);
    PathFlag patflag = File::CheckPathFlag(pat);

    fmt::println("source path : {} {}", src, path_flag_colorful(srcflag));
    fmt::println("patch path : {} {}", pat, path_flag_colorful(patflag));

    if (srcflag == PathFlag::NOT_EXISTS || patflag == PathFlag::NOT_EXISTS) {
        fmt::perrorln_red("source or patch not exists");
        return 1;
    }

    if (patflag != PathFlag::FILE) {
        fmt::perrorln_red("patch is not a file");
        return 1;
    }

    g_tempfolder = File::CombinePath(File::GetParentPath(pat), APP_TEMP);
    g_workfolder = File::CombinePath(g_tempfolder, "work");
    g_keeptemp   = sub_patch->get_option("-k")->as<bool>();

    fmt::println("temp folder : {}", g_tempfolder);

    if (File::IsPathExists(g_tempfolder)) {
        File::Remove(g_tempfolder);
    }
    if (!File::Mkdir(g_tempfolder)) {
        fmt::perrorln_red("make temp folder failed : {}", g_tempfolder);
        return 1;
    }

    File::HidePath(g_tempfolder);

    // clang-format off

    // extract patch 
    SPINNER_RUN("Extract patch", [&] {
        return !ToolBoxInstance()._7z().unzip(pat, g_tempfolder);
    },
    RunMsgMap{
        MAKE_RUNMSG(1, true, fmt::color::red, "extract patch failed, maybe it's not a valid patch"),
    });
    
    // load metadata
    SPINNER_RUN("Loading metadata", [&] {
        return !metadata.deserialize(File::CombinePath(g_workfolder, "metadata"));
    },
    RunMsgMap{
        MAKE_RUNMSG(1, true, fmt::color::red, "load metadata failed, maybe it's not a valid patch"),
    });

    return do_patch(PatchProgramArgs(
        sub_patch->get_option("-i")->as<bool>(),
        sub_patch->get_option("--backup")->as<std::string>(),
        src,
        g_workfolder,
        metadata
    ));

    // clang-format on
}

int patch_pe_program(CLI::App& app, PatchPE& pe)
{
    DiffMetaData metadata;
    std::string  pat;

    std::string src     = File::GetFullPath(app.get_option("source")->as<std::string>());
    PathFlag    srcflag = File::CheckPathFlag(src);

    fmt::println("source path : {} {}", src, path_flag_colorful(srcflag));

    if (srcflag == PathFlag::NOT_EXISTS) {
        fmt::perrorln_red("source not exists");
        return 1;
    }

    if (srcflag == PathFlag::FILE) {
        g_tempfolder = File::CombinePath(File::GetParentPath(src), APP_TEMP);
    }
    else {
        g_tempfolder = File::CombinePath(src, APP_TEMP);
    }
    g_workfolder = File::CombinePath(g_tempfolder, "work");
    g_keeptemp   = app.get_option("-k")->as<bool>();

    fmt::println("temp folder : {}", g_tempfolder);

    if (File::IsPathExists(g_tempfolder)) {
        File::Remove(g_tempfolder);
    }
    if (!File::Mkdir(g_tempfolder)) {
        fmt::perrorln_red("make temp folder failed : {}", g_tempfolder);
        return 1;
    }

    File::HidePath(g_tempfolder);

    // clang-format off

    // export patch
    pat = File::CombinePath(g_tempfolder, (std::string("temp") + FPToolXdelta::PATCH_EXT));
    SPINNER_RUN("Export patch", [&] {
        return !pe.ExtractPatch(pat);
    },
    RunMsgMap{
        MAKE_RUNMSG(1, true, fmt::color::red, fmt::gen("export patch file failed : {}", pat)),
    });

    // extract patch 
    SPINNER_RUN("Extract patch", [&] {
        return !ToolBoxInstance()._7z().unzip(pat, g_tempfolder);
    },
    RunMsgMap{
        MAKE_RUNMSG(1, true, fmt::color::red, "extract patch failed, maybe it's not a valid patch"),
    });
    
    // load metadata
    SPINNER_RUN("Loading metadata", [&] {
        return !metadata.deserialize(File::CombinePath(g_workfolder, "metadata"));
    },
    RunMsgMap{
        MAKE_RUNMSG(1, true, fmt::color::red, "load metadata failed, maybe it's not a valid patch"),
    });

    return do_patch(PatchProgramArgs(
        app.get_option("-i")->as<bool>(),
        app.get_option("--backup")->as<std::string>(),
        src,
        g_workfolder,
        metadata
    ));

    // clang-format on
}

void exit_handler()
{
    if (g_ctrlc) {
        fmt::perrorln_red("you <ctrl-c> to stop the program, so keep the temporary files");
    }
    else if (!g_tempfolder.empty() && !g_keeptemp) {
        File::Remove(g_tempfolder);
    }
}

void signal_handler(int signum)
{
    switch (signum) {
        case SIGINT:
            g_ctrlc = true;
            std::exit(1);
            break;
        default:
            break;
    }
}

int main(int argc, char** argv)
{
    CLI::App app{APP_NAME " " APP_DESCRIPTION, APP_PROGRAM};
    PatchPE  pe;

    app.set_version_flag("-v, --version", APP_VERSION);

    std::atexit(exit_handler);
    std::signal(SIGINT, signal_handler);

    if (!ToolBoxInstance().setup(APP_NAME)) {
        fmt::perrorln_red("extract tools failed, please check directory : {}",
                          fmt::green_str(File::GetApplicationFolder(APP_NAME)));
        return 1;
    }

    if (pe.IsHasPatch()) {
        app.description(APP_NAME " " APP_DESCRIPTION " " APP_PATCHER);
        app.add_flag("-k, --keep", "keep temp");
        app.add_flag("-i, --ignore-backup", "don't need backup");
        app.add_option("--backup", "specify backup file path")->default_str("");
        app.add_option("source", "source file or directory path")->default_str(".")->required(false);
        CLI11_PARSE(app, argc, argv);
        fmt::println("please ensure that the specified path is not being used by any other application");
        return patch_pe_program(app, pe);
    }

    auto sub_diff  = app.add_subcommand("diff", "compare src and target, generate patch");
    auto sub_patch = app.add_subcommand("patch", "apply patch");

    sub_diff->add_flag("-k, --keep", "keep temp");
    sub_diff->add_flag("-e, --output-execute", "output an executable file");
    sub_diff->add_flag("-f, --force", "force overwrite patch");
    sub_diff->add_option("source", "source file or directory path")->required(true);
    sub_diff->add_option("target", "target file or directory path")->required(true);
    sub_diff->add_option("patch", "patch output path, no need to specify suffix name")->required(true);
    sub_diff->validate_positionals();

    sub_patch->add_flag("-k, --keep", "keep temp");
    sub_patch->add_flag("-i, --ignore-backup", "don't need backup");
    sub_patch->add_option("--backup", "specify backup file path")->default_str("");
    sub_patch->add_option("source", "source file or directory path")->required(true);
    sub_patch->add_option("patch", "patch file path")->required(true);
    sub_patch->validate_positionals();

    app.require_subcommand();

    CLI11_PARSE(app, argc, argv);
    fmt::println("please ensure that the specified path is not being used by any other application");

    if (app.got_subcommand(sub_diff)) {
        return diff_program(sub_diff, pe);
    }
    else if (app.got_subcommand(sub_patch)) {
        return patch_program(sub_patch);
    }

    return 0;
}
