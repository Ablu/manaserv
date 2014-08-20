import qbs 1.0

import BuildHelper

Test {
    testFor: "ManaPersitenceSql"
    suffix: "Sqlite"

    Depends {
        name: "Qt"
        submodules: [
            "sql",
        ]
    }

    Depends {
        name: "ManaEntities"
    }

    cpp.rpaths: BuildHelper.buildRpathForPrefix(project, qbs.targetOS, "../..")
}
