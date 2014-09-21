import qbs 1.0

DynamicLibrary {
    Depends {
        name: "cpp"
    }

    Group {
        name: "Binaries"
        fileTagsFilter: "dynamiclibrary"
        qbs.install: true
        qbs.installDir: project.libDir
    }

    cpp.cxxFlags: [
        "-std=c++0x",
        "-Wall",
        "-Werror",
    ]
}
