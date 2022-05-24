#include <vcpkg/base/cofffilereader.h>
#include <vcpkg/base/files.h>
#include <vcpkg/base/system.print.h>
#include <vcpkg/base/system.process.h>
#include <vcpkg/base/util.h>

#include <vcpkg/build.h>
#include <vcpkg/class.postbuildlint.h>
#include <vcpkg/packagespec.h>
#include <vcpkg/postbuildlint.buildtype.h>
#include <vcpkg/vcpkgpaths.h>

namespace
{
    using namespace vcpkg;

    DECLARE_AND_REGISTER_MESSAGE(PostBuildLintIncludeInCmakeHelper,
                                 (msg::path),
                                 "",
                                 "The folder {path} exists in a cmake helper port. This is incorrect since only cmake "
                                 "files should be installed.");
    DECLARE_AND_REGISTER_MESSAGE(
        PostBuildLintMissingIncludeDir,
        (msg::path),
        "",
        "The folder {path} is empty or not present. This indicates the package was not correctly installed.");
    DECLARE_AND_REGISTER_MESSAGE(
        PostBuildLintRestrictedHeaders,
        (),
        "",
        "Restricted header paths are present. These files can prevent the core C/C++ runtime and other packages from compiling correctly:");
    DECLARE_AND_REGISTER_MESSAGE(
        PostBuildLintDisableRestrictedHeaders,
        (msg::cmake_var),
        "",
        "In exceptional circumstances, this policy can be disabled via {cmake_var}");
}

namespace vcpkg::PostBuildLint
{
    // These files are taken from the libc6-dev package on Ubuntu inside /usr/include/x86_64-linux-gnu/sys/
    static constexpr StringLiteral restricted_sys_filenames[] = {
        "acct.h",      "auxv.h",        "bitypes.h",  "cdefs.h",    "debugreg.h",  "dir.h",         "elf.h",
        "epoll.h",     "errno.h",       "eventfd.h",  "fanotify.h", "fcntl.h",     "file.h",        "fsuid.h",
        "gmon.h",      "gmon_out.h",    "inotify.h",  "io.h",       "ioctl.h",     "ipc.h",         "kd.h",
        "klog.h",      "mman.h",        "mount.h",    "msg.h",      "mtio.h",      "param.h",       "pci.h",
        "perm.h",      "personality.h", "poll.h",     "prctl.h",    "procfs.h",    "profil.h",      "ptrace.h",
        "queue.h",     "quota.h",       "random.h",   "raw.h",      "reboot.h",    "reg.h",         "resource.h",
        "select.h",    "sem.h",         "sendfile.h", "shm.h",      "signal.h",    "signalfd.h",    "socket.h",
        "socketvar.h", "soundcard.h",   "stat.h",     "statfs.h",   "statvfs.h",   "stropts.h",     "swap.h",
        "syscall.h",   "sysctl.h",      "sysinfo.h",  "syslog.h",   "sysmacros.h", "termios.h",     "time.h",
        "timeb.h",     "timerfd.h",     "times.h",    "timex.h",    "ttychars.h",  "ttydefaults.h", "types.h",
        "ucontext.h",  "uio.h",         "un.h",       "unistd.h",   "user.h",      "ustat.h",       "utsname.h",
        "vfs.h",       "vlimit.h",      "vm86.h",     "vt.h",       "vtimes.h",    "wait.h",        "xattr.h",
    };
    // These files are taken from the libc6-dev package on Ubuntu inside the /usr/include/ folder
    static constexpr StringLiteral restricted_crt_filenames[] = {
        "_G_config.h", "aio.h",      "aliases.h",     "alloca.h",    "ar.h",       "argp.h",         "argz.h",
        "assert.h",    "byteswap.h", "complex.h",     "cpio.h",      "crypt.h",    "ctype.h",        "dirent.h",
        "dlfcn.h",     "elf.h",      "endian.h",      "envz.h",      "err.h",      "errno.h",        "error.h",
        "execinfo.h",  "fcntl.h",    "features.h",    "fenv.h",      "fmtmsg.h",   "fnmatch.h",      "fstab.h",
        "fts.h",       "ftw.h",      "gconv.h",       "getopt.h",    "glob.h",     "gnu-versions.h", "grp.h",
        "gshadow.h",   "iconv.h",    "ifaddrs.h",     "inttypes.h",  "langinfo.h", "lastlog.h",      "libgen.h",
        "libintl.h",   "libio.h",    "limits.h",      "link.h",      "locale.h",   "malloc.h",       "math.h",
        "mcheck.h",    "memory.h",   "mntent.h",      "monetary.h",  "mqueue.h",   "netash",         "netdb.h",
        "nl_types.h",  "nss.h",      "obstack.h",     "paths.h",     "poll.h",     "printf.h",       "proc_service.h",
        "pthread.h",   "pty.h",      "pwd.h",         "re_comp.h",   "regex.h",    "regexp.h",       "resolv.h",
        "sched.h",     "search.h",   "semaphore.h",   "setjmp.h",    "sgtty.h",    "shadow.h",       "signal.h",
        "spawn.h",     "stab.h",     "stdc-predef.h", "stdint.h",    "stdio.h",    "stdio_ext.h",    "stdlib.h",
        "string.h",    "strings.h",  "stropts.h",     "syscall.h",   "sysexits.h", "syslog.h",       "tar.h",
        "termio.h",    "termios.h",  "tgmath.h",      "thread_db.h", "time.h",     "ttyent.h",       "uchar.h",
        "ucontext.h",  "ulimit.h",   "unistd.h",      "ustat.h",     "utime.h",    "utmp.h",         "utmpx.h",
        "values.h",    "wait.h",     "wchar.h",       "wctype.h",    "wordexp.h",
    };
    // These files are general names that have shown to be problematic in the past
    static constexpr StringLiteral restricted_general_filenames[] = {
        "json.h",
        "parser.h",
        "lexer.h",
        "config.h",
        "local.h",
        "slice.h",
        "platform.h",
    };
} // namespace vcpkg::PostBuildLint

namespace vcpkg::PostBuildLint
{
    PostBuildLint::PostBuildLint(const PackageSpec& spec,
                                 const VcpkgPaths& paths,
                                 const Build::PreBuildInfo& pre_build_info,
                                 const Build::BuildInfo& build_info,
                                 const Path& port_dir)
        : m_spec(spec)
        , m_paths(paths)
        , m_fs(m_paths.get_filesystem())
        , m_pre_build_info(pre_build_info)
        , m_build_info(build_info)
        , m_port_dir(port_dir)
        , m_package_dir(m_paths.package_dir(m_spec))
    {
    }

    PostBuildLint::~PostBuildLint() { }

    size_t PostBuildLint::perform_all_checks()
    {
        // Don't run checks if port is empty
        if (m_build_info.policies.is_enabled(vcpkg::Build::BuildPolicy::EMPTY_PACKAGE))
        {
            return 0;
        }

        // for dumpbin
        const Toolset& toolset = m_paths.get_toolset(m_pre_build_info);
        const auto package_dir = m_paths.package_dir(m_spec);
    }

    LintStatus PostBuildLint::check_for_files_in_include_directory()
    {
        const auto include_dir = m_package_dir / "include";

        if (m_build_info.policies.is_enabled(Build::BuildPolicy::CMAKE_HELPER_PORT))
        {
            if (m_fs.exists(include_dir, IgnoreErrors{}))
            {
                msg::print_warning(msgPostBuildLintIncludeInCmakeHelper, msg::path = "/include");
                return LintStatus::PROBLEM_DETECTED;
            }
            else
            {
                return LintStatus::SUCCESS;
            }
        }

        if (!m_fs.exists(include_dir, IgnoreErrors{}) || m_fs.is_empty(include_dir, IgnoreErrors{}))
        {
            msg::print_warning(msgPostBuildLintMissingIncludeDir, msg::path = "/include");
            return LintStatus::PROBLEM_DETECTED;
        }

        return LintStatus::SUCCESS;
    }

    LintStatus PostBuildLint::check_for_restricted_include_files()
    {
        static constexpr Span<const StringLiteral> restricted_lists[] = {
            restricted_sys_filenames,
            restricted_crt_filenames,
            restricted_general_filenames
        };
        const auto include_dir = m_package_dir / "include";
        const auto files = m_fs.get_files_non_recursive(include_dir, IgnoreErrors{});
        std::unordered_set<StringView> filenames_s;
        for (auto&& file : files)
        {
            filenames_s.insert(file.filename());
        }

        std::vector<Path> violations;
        for (auto&& flist : restricted_lists)
            for (auto&& f : flist)
            {
                if (Util::Sets::contains(filenames_s, f))
                {
                    violations.push_back(Path("include") / f);
                }
            }

        if (!violations.empty())
        {
            std::lock_guard lock(m_mutex);
            
            msg::print_warning(msgPostBuildLintRestrictedHeaders);
            print_paths(violations);
            msg::println(msgPostBuildLintDisableRestrictedHeaders,
                         msg::cmake_var = Build::to_cmake_variable(Build::BuildPolicy::ALLOW_RESTRICTED_HEADERS));
            
            return LintStatus::PROBLEM_DETECTED;
        }

        return LintStatus::SUCCESS;
    }

    std::future<LintStatus> PostBuildLint::check_for_restricted_include_files_async()
    {
        return std::async(std::launch::async, &PostBuildLint::check_for_restricted_include_files, this);
    }
}
