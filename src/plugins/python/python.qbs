import qbs 1.0

QtcPlugin {
    name: "Python"

    Depends { name: "Qt.widgets" }

    Depends { name: "QmlJS" }
    Depends { name: "Utils" }

    Depends { name: "Core" }
    Depends { name: "TextEditor" }
    Depends { name: "ProjectExplorer" }
    Depends { name: "LanguageClient" }
    Depends { name: "LanguageServerProtocol" }

    Group {
        name: "General"
        files: [
            "pipsupport.cpp",
            "pipsupport.h",
            "pyside.cpp",
            "pyside.h",
            "pysidebuildconfiguration.cpp",
            "pysidebuildconfiguration.h",
            "pysideuicextracompiler.cpp",
            "pysideuicextracompiler.h",
            "python.qrc",
            "pythonconstants.h",
            "pythoneditor.cpp",
            "pythoneditor.h",
            "pythonformattoken.h",
            "pythonhighlighter.cpp",
            "pythonhighlighter.h",
            "pythonindenter.cpp",
            "pythonindenter.h",
            "pythonlanguageclient.cpp",
            "pythonlanguageclient.h",
            "pythonplugin.cpp",
            "pythonplugin.h",
            "pythonproject.cpp",
            "pythonproject.h",
            "pythonrunconfiguration.cpp",
            "pythonrunconfiguration.h",
            "pythonscanner.cpp",
            "pythonscanner.h",
            "pythonsettings.cpp",
            "pythonsettings.h",
            "pythonutils.cpp",
            "pythonutils.h",
        ]
    }
}
