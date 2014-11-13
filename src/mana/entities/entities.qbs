import qbs 1.0

DynamicCppLibrary {
    name: "ManaEntities"

    Depends {
        name: "Qt"
        submodules: [
            "core",
        ]
    }

    Group {
        name: "Sources"
        files: [
            "account.cpp",
            "account.h",
            "character.cpp",
            "character.h",
            "guild.cpp",
            "guild.h",
            "map.cpp",
            "map.h",
            "post.cpp",
            "post.h",
        ]
    }

    cpp.includePaths: [
        "../../",
    ]
}
