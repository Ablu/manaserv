import qbs 1.0

DynamicCppLibrary {
    name: "ManaMapReaderTmx"

    Depends {
        name: "Qt"
        submodules: [
            "core",
        ]
    }

    Depends {
        name: "ManaEntities"
    }

    Depends {
        name: "libtiled"
    }

    Group {
        name: "Sources"
        files: [
            "tmxreader.cpp",
            "tmxreader.h",
        ]
    }

    cpp.includePaths: [
        ".",
        "/usr/include/",
        "../../../",
        "../../../3rdParty/Tiled/src/",
    ]
}
