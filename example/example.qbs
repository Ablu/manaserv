import qbs 1.0

Product {
    name: "Example game data"

    Group {
        name: "Configuration files"
        files: [
            "*.xml",
        ]
        qbs.install: true
        qbs.installDir: "share/manaserv/example/"
    }

    Group {
        name: "Map files"
        files: [
            "maps/*.tmx",
        ]
        qbs.install: true
        qbs.installDir: "share/manaserv/example/maps/"
    }
}
