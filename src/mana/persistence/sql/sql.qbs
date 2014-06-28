import qbs 1.0

DynamicLibrary {
    name: "ManaPersitenceSql"

    Depends {
        name: "Qt"
        submodules: [
            "sql",
        ]
    }

    Depends {
        name: "cpp"
    }

    Depends {
        name: "ManaPersitenceInterfaces"
    }

    Depends {
        name: "ManaEntities"
    }

    Group {
        name: "Sources"
        files: [
            "sqlstorage.h",
            "sqlstorage.cpp",
        ]
    }

    cpp.includePaths: [
        "../../../",
    ]

    cpp.cxxFlags: [
        "-std=c++0x",
    ]
}
