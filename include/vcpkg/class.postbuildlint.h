#pragma once

#include <future>

#include <vcpkg/fwd/build.h>
#include <vcpkg/fwd/vcpkgpaths.h>

#include <vcpkg/base/files.h>

namespace vcpkg
{
    struct PackageSpec;
}

namespace vcpkg::PostBuildLint
{
    enum class LintStatus
    {
        SUCCESS = 0,
        PROBLEM_DETECTED = 1
    };

    inline static void operator+=(size_t& left, const LintStatus& right) { left += static_cast<size_t>(right); }


    class PostBuildLint
    {
    public:
        PostBuildLint(const PackageSpec& spec,
                      const VcpkgPaths& paths,
                      const Build::PreBuildInfo& pre_build_info,
                      const Build::BuildInfo& build_info,
                      const Path& port_dir);
        ~PostBuildLint();

        size_t perform_all_checks();

        LintStatus check_for_files_in_include_directory();
        std::future<LintStatus> check_for_restricted_include_files_async();
        LintStatus check_for_restricted_include_files();

    private:
        const PackageSpec m_spec;
        const VcpkgPaths& m_paths;
        const vcpkg::Filesystem& m_fs;
        const Build::PreBuildInfo& m_pre_build_info;
        const Build::BuildInfo m_build_info;
        const Path m_port_dir;
        const Path m_package_dir;
        std::mutex m_mutex;
    };
}
