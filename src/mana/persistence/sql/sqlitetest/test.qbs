import qbs 1.0

import BuildHelper

Test {
    testFor: "ManaPersitenceSql"
    suffix: "Sqlite"

    cpp.rpaths: BuildHelper.buildRpathForPrefix(project, qbs.targetOS, "../..")
}
