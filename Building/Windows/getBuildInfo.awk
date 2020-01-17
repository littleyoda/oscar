#
# awk script to extract build identification from build_number.h, git_info.h, and version.h
# for use by Inno Setup in building installation file for OSCAR.  See DEPLOY.BAT for information.
#
# Usage: gawk -f getBuildInfo.awk build_number.h >buildInfo.iss
#        gawk -f getBuildInfo.awk git_info.h >>buildInfo.iss
#        gawk -f getBuildInfo.awk version.h >>buildInfo.iss
#        echo %cd% | gawk -f %sourcedir%getBuildInfo.awk >>buildInfo.iss

/#define GIT_BRANCH / { print "#define MyGitBranch", $3 }
/#define GIT_REVISION / { print "#define MyGitRevision", $3 }
/#define GIT_TAG / { print "#define MyGitTag", $3 }

/#define VERSION / {
    version = $3
    print "#define MyAppVersion", version

    split(version, v, "[\.\-]")
    status = v[4] ? v[4] : "r"
    print "#define MyReleaseStatus \"" status "\""
    
    split("alpha beta rc r", parts, " ")
    for (i=1; i <= length(parts); i++) dict[parts[i]]=i
    build = dict[status]
    print "#define MyBuildNumber \"" (build * 32) "\""
}

/32.*bit/ { print "#define MyPlatform \"Win32\"" }
/64.*bit/ { print "#define MyPlatform \"Win64\"" }
