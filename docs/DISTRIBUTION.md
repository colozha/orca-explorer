# Source and binary distribution

Orca Explorer 0.1.0 is a development version based on Dolphin 26.08.2. Publish to https://github.com/colozha/orca-explorer only after maintainer review; these instructions do not imply that a remote repository or release exists.

## Source

Ship all source, build files, resource files, translations, LICENSE, COPYING, COPYING.DOC, LICENSES, REUSE.toml and asset attribution. Retain upstream notices and record fork modifications. An ordinary source archive may omit `.git`; the prepared working copy deliberately retains full history and uncommitted changes. Do not ship build outputs or developer machine caches as source.

## Binary

Build from the corresponding published source, preserve the complete license notices and make the corresponding source available as required by GPL-3.0-only. Package dependencies separately under their own licenses. The install prefix must not overwrite Dolphin files. Review the CMake install manifest and test alongside the distribution's Dolphin package.

- Executable: `orca-explorer`; desktop/app ID: `io.github.colozha.OrcaExplorer`.
- Libraries: private `lib[64]/orca-explorer`, with relative RPATHs. VCS class ABI and SONAME remain compatible with upstream plugins; SDK installation uses the OrcaExplorer namespace.
- Bundled KParts and KCM modules, catalogs, helper executables, update scripts, icons, metadata and shell completion have separate names or private destinations.
- FileManager1 methods are exposed on Orca's application bus; the global `org.freedesktop.FileManager1` name is not claimed. Default MIME/file-manager associations are not changed.
- Settings: `orca-explorerrc`, state `orca-explorerstaterc`; data/cache are under the application name. Bookmarks and Places use application storage. View properties use `OrcaExplorer`/`OrcaExplorerSettings` groups and a separate metadata key, preserving other `.directory` entries.
- Shared desktop services (KIO, Solid, Baloo, portal dialogs), external service menus and VCS plugins retain their existing integration contracts.

The Flatpak manifest retains KDE runtime/dependency requirements. Build it from the fork source; validate with flatpak-builder before publication. Do not add global FileManager1 ownership or change default associations.

For rebuilding compatible VCS plugins, use `find_package(OrcaExplorerVcs REQUIRED)` and link the retained `DolphinVcs` target. The relocated SDK keeps `<Dolphin/KVersionControlPlugin>` source includes under `include/OrcaExplorer/Dolphin`, without installing into the system Dolphin include directory. Existing compiled VCS plugins retain their ABI. The Flatpak manifest adapts the upstream plugin package lookup to the fork SDK.
