import qbs 1.0

Project {
    property bool enableRPath: true
    property string libDir: "lib"
    property stringList rpaths: {
    }


    references: [
        "src/src.qbs"
    ]

    qbsSearchPaths: [
        "qbs_imports/",
    ]
}
