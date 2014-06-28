import qbs 1.0

import BuildHelper

Test {
    testFor: "ManaConfigurationXmlConfiguration"

    cpp.rpaths: BuildHelper.buildRpathForPrefix(project, qbs.targetOS, "../..")
}
