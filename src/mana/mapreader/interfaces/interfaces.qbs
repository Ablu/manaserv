import qbs 1.0

DynamicCppLibrary {
    name: "ManaMapReaderInterfaces"

    Depends {
        name: "ManaEntities"
    }

    Depends {
        name: "Qt"
        submodules: [
            "core",
        ]
    }

    cpp.includePaths: [
        ".",
        "../../../"
    ]

    Group {
        name: "Sources"
        files: [
            "imapreader.h",
        ]
    }
}
