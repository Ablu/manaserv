import qbs 1.0

import BuildHelper

Test {
    testFor: "ManaMapReaderTmx"

    Depends {
        name: "ManaEntities"
    }

    Depends {
        name: "Qt"
        submodules: ["gui", "widgets"]
    }

    cpp.rpaths: BuildHelper.buildRpathForPrefix(project, qbs.targetOS, "../..")
    cpp.defines: ['QT_WIDGETS_LIB']
}
