import qbs 1.0

Project {
    references: [
        "sqlitetest/test.qbs"
    ]

    DynamicCppLibrary {
        name: "ManaPersitenceSql"

        Depends {
            name: "Qt"
            submodules: [
                "core",
                "sql",
            ]
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
    }
}
