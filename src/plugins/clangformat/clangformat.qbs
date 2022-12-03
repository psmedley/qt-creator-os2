import qbs

QtcPlugin {
    name: "ClangFormat"
    targetName: "ClangFormatPlugin"

    Depends { name: "Core" }
    Depends { name: "CppEditor" }
    Depends { name: "ProjectExplorer" }
    Depends { name: "TextEditor" }
    Depends { name: "Utils" }

    Depends { name: "libclang"; required: false }
    Depends { name: "clang_defines" }

    Depends { name: "Qt.widgets" }

    condition: libclang.present
               && libclang.llvmFormattingLibs.length
               && (!qbs.targetOS.contains("windows") || libclang.llvmBuildModeMatches)

    cpp.cxxFlags: base.concat(libclang.llvmToolingCxxFlags)
    cpp.linkerFlags: {
        var flags = base;
        if (qbs.targetOS.contains("unix") && !qbs.targetOS.contains("macos"))
            flags.push("--exclude-libs", "ALL");
        return flags;
    }
    cpp.includePaths: base.concat(libclang.llvmIncludeDir)
    cpp.libraryPaths: base.concat(libclang.llvmLibDir)
    cpp.dynamicLibraries: base.concat(libclang.llvmFormattingLibs)
    cpp.rpaths: base.concat(libclang.llvmLibDir)

    files: [
        "clangformatbaseindenter.h",
        "clangformatbaseindenter.cpp",
        "clangformatchecks.ui",
        "clangformatconfigwidget.cpp",
        "clangformatconfigwidget.h",
        "clangformatconfigwidget.ui",
        "clangformatconstants.h",
        "clangformatfile.cpp",
        "clangformatfile.h",
        "clangformatindenter.cpp",
        "clangformatindenter.h",
        "clangformatplugin.cpp",
        "clangformatplugin.h",
        "clangformatsettings.cpp",
        "clangformatsettings.h",
        "clangformatutils.h",
        "clangformatutils.cpp",
    ]

    Group {
        name: "Tests"
        prefix: "tests/"
        condition: qtc.testsEnabled
        cpp.defines: outer.concat('TESTDATA_DIR="' + sourceDirectory + "/tests/data" + '"')
        files: [
            "clangformat-test.cpp",
            "clangformat-test.h",
            "data/.clang-format",
        ]
    }
}
