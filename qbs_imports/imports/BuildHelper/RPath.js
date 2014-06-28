function buildRpathForPrefix(topLevelProject, targetOS, prefix) {
    if (!topLevelProject.enableRPath)
        return undefined;
    if (targetOS.contains("linux"))
        return ["$ORIGIN/" + prefix + "/" + topLevelProject.libDir];
    if (targetOS.contains("osx"))
        return ["@loader_path/" + prefix + "/" + topLevelProject.libDir]
}
