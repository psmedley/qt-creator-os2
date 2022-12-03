QtcPlugin {
    name: "ImageViewer"

    Depends { name: "Qt.svg"; required: false }
    Depends { name: "Qt.svgwidgets"; condition: usesQt6; required: false }
    Depends { name: "Qt.widgets" }
    Depends { name: "Utils" }

    Depends { name: "Core" }

    Properties {
        condition: !Qt.svg.present || (usesQt6 && !Qt.svgwidgets.present)
        cpp.defines: base.concat("QT_NO_SVG")
    }

    files: [
        "exportdialog.cpp",
        "exportdialog.h",
        "multiexportdialog.cpp",
        "multiexportdialog.h",
        "imageview.cpp",
        "imageview.h",
        "imageviewer.cpp",
        "imageviewer.h",
        "imageviewerconstants.h",
        "imageviewerfactory.cpp",
        "imageviewerfactory.h",
        "imageviewerfile.cpp",
        "imageviewerfile.h",
        "imageviewerplugin.cpp",
        "imageviewerplugin.h",
        "imageviewertoolbar.ui",
    ]
}
