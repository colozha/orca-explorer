# Validation — 2026-10-07

Orca Explorer **0.1.0 (development)**, based on Dolphin **26.08.2**. No repository, tag or release was published by this preparation.

## Environment and results

Fedora 44, Qt 6.11.2, KDE Frameworks 6.30.0, Baloo Widgets release/26.08 source reporting 26.08.2. Development packages were extracted into a temporary prefix; no system packages or installed Dolphin files were replaced.

- CMake/Ninja build with Baloo/information panel and terminal support: passed.
- All **21 CTest suites**: passed. These include item model/view/controller/selection, view properties, search, main window, drag and drop, Places, smoke launch, and both appearance modes.
- Appearance tests cover GNOME default/reset, saved preferences, independent config/state/Places/bookmarks, private plugin discovery and actions, symbolic palette changes, complete icon aliases, toolbar rebuilding/relocation, split views, tabs and close confirmation.
- Native GNOME Wayland appearance suite: **10 passed, 0 failed**; the optional screenshot case is skipped unless requested explicitly.
- Dolphin and installed Orca running simultaneously in isolated XDG/D-Bus session: independent routing in both directions, private library loading, separate storage, no global FileManager1 ownership by Orca, and independent closing passed. The unique Orca daemon and FileManager1 ShowFolders dispatch were also verified.
- Relocated VCS SDK: a consumer using the retained `<Dolphin/KVersionControlPlugin>` header and `DolphinVcs` target built and ran against `OrcaExplorerVcs`.
- Desktop-file and AppStream validation: passed. REUSE lint: passed. CMake install manifest: no collision with installed Dolphin package paths.
- Original Dolphin source checksums matched the pre-copy baseline. The destination Git history was subsequently reset for independent publication; upstream provenance and credits are preserved.

## Actual screenshots

The captures use the actual DolphinMainWindow implementation with the Orca branding and controlled fixture files, not mockups or Nautilus reference images. The main 100% captures use native GNOME Wayland; 200% captures use Qt scaling under Xvfb. Test fixtures cover long names, empty folders, hidden files, image files, symlinks and split views. KIO thumbnails depend on installed thumbnailer packages; an image file can therefore appear with its MIME icon in these fixtures.

| Appearance | 1920×1033 logical pixels | 1024×768 logical pixels |
| --- | --- | --- |
| Light, 100% | [Capture](screenshots/orca-GnomeLight-1920x1033-1x.png) | [Capture](screenshots/orca-GnomeLight-1024x768-1x.png) |
| Dark, 100% | [Capture](screenshots/orca-GnomeDark-1920x1033-1x.png) | [Capture](screenshots/orca-GnomeDark-1024x768-1x.png) |
| Light, 200% | [Capture](screenshots/orca-GnomeLight-1920x1033-2x.png) | [Capture](screenshots/orca-GnomeLight-1024x768-2x.png) |
| Dark, 200% | [Capture](screenshots/orca-GnomeDark-1920x1033-2x.png) | [Capture](screenshots/orca-GnomeDark-1024x768-2x.png) |

[Split view](screenshots/orca-split.png). Original SVG and all eight PNG sizes were inspected for readability, including small sizes and light/dark backgrounds.

## Remaining manual/environment checks

Real pointer-driven Wayland system move, all resize edges/corners, snapping, dragging across windows and multiple-window compositor behavior remain manual checks. A synthetic Qt event cannot supply the compositor input serial required by these gestures. Wider accessibility/translated-label checks, read-only remote locations, packaged thumbnailers, Flatpak packaging and Windows/macOS builds also require their own test environments. The prepared Flatpak manifest has been checked structurally, not built end-to-end.

## Destination verification

The complete staging tree, including hidden files and `.git`, was copied to `/home/husain/Documents/Works/Github/orca-explorer`. A fresh CMake/Ninja build from this directory completed, and all **21/21 suites** passed again. Installation into a temporary prefix produced **397 files**, with zero collisions against the installed Dolphin package. Installed application/private-library/KPart RPATHs are relative and contain no temporary development prefix. The relocated SDK consumer built and ran; final installed application routing and unique-daemon protocol checks passed.

On **2026-10-07**, destination Git metadata was replaced with a new repository on branch `master`, containing one commit named `Initial commit: Orca Explorer` and no inherited tags. Its only remote is `origin`, pointing to `https://github.com/colozha/orca-explorer.git`. No GitHub repository was created and no push was performed. The upstream baseline remains `44902d086cf3a795c680887524938f1b2caf3c93`, recorded in provenance rather than inherited Git history. Source/assets and licensing files were preserved; the original Dolphin directory, including its Git history, was left unchanged. The `build/` directory contains local validation artifacts and is ignored by Git.

Desktop/AppStream validation, REUSE lint and whitespace checks passed at the destination. The final CLI author list contains colozha as fork maintainer and retains every upstream author, with roles explicitly identified as upstream.

