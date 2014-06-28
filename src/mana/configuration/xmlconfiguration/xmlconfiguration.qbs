import qbs 1.0

DynamicLibrary {
    name: "ManaConfigurationXmlConfiguration"

    Depends {
        name: "cpp"
    }

    Depends {
        name: "Qt"
        submodules: [
            "core",
        ]
    }

    Group {
        name: "Sources"
        files: [
            "xmlconfiguration.cpp",
            "xmlconfiguration.h",
        ]
    }

    Group {
        name: "Utils"
        prefix: "../../../utils/"
        files: [
            "string.cpp",
            "string.h",
        ]
    }

    cpp.includePaths: [
        ".",
        "/usr/include/libxml2/",
        "/usr/include/",
        "../../../"
    ]

    cpp.dynamicLibraries: [
        "xml2",
    ]

    cpp.cxxFlags: [
        "-std=c++0x",
    ]
}
