import qbs 1.0

DynamicLibrary {
    name: "ManaEntities"



    Depends {
        name: "cpp"
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
            "post.cpp",
            "post.h",
        ]
    }

    cpp.includePaths: [
        "../../",
    ]

    cpp.cxxFlags: [
        "-std=c++0x",
    ]
}
