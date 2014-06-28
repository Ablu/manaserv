import qbs 1.0

CppApplication {
    property string testFor

    name: testFor + "Test"

    property string testInstallPrefix: "tests/" + testFor

    Depends {
        name: "Qt"
        submodules: [
            "test",
        ]
    }

    Depends {
        name: testFor
    }

    Group {
        name: "Binaries"
        fileTagsFilter: "application"
        qbs.install: true
        qbs.installDir: testInstallPrefix
    }

    Group {
        name: "Sources"
        files: [
            "*.cpp",
            "*.h",
        ]
    }

    Group {
        name: "Test data"
        prefix: "testdata/"
        qbs.install: true
        qbs.installDir: testInstallPrefix + "/testdata/"
        files: [
            "*"
        ]
    }

    cpp.includePaths: [
        "../../../../",
    ]
}
