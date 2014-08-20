import qbs 1.0

Product {
    Group {
        name: "Scripts"
        files: [
            "createTables.sql",
        ]
        qbs.install: true
        qbs.installDir: "share/manaserv/sql/sqlite/"
    }
}
