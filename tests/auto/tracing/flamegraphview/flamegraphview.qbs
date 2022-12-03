import qbs
import "../tracingautotest.qbs" as TracingAutotest

TracingAutotest {
    name: "FlameGraphView autotest"

    Depends { name: "Utils" }
    Depends { name: "Qt.quickwidgets" }

    Group {
        name: "Test sources"
        files: [
            "testflamegraphmodel.h",
            "tst_flamegraphview.cpp", "flamegraphview.qrc"
        ]
    }
}
