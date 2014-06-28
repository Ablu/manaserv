import qbs 1.0

DynamicLibrary {
    name: "ManaPersitenceInterfaces"

    Depends {
        name: "Qt"
        submodules: [
            "core",
        ]
    }

    Group {
        name: "Sources"
        files: [
            "istorage.h",
        ]
    }
}
