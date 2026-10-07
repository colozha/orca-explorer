<!-- SPDX-FileCopyrightText: 2026 Orca Explorer Contributors -->
<!-- SPDX-License-Identifier: CC0-1.0 -->
# Desktop file manager and Open/Save integration

Orca can be selected as the file manager for one Linux desktop account. Its new
FileChooser portal provides an Orca dialog to applications using XDG Desktop
Portal. Applications with their own embedded file picker continue to use that
picker. Open/Save integration is separate from the default directory association.

## Build and install

A normal installation remains alongside Dolphin and does not change defaults.
Build with `-DORCA_INSTALL_DEFAULT_FILE_MANAGER=ON` to install the optional
`org.freedesktop.FileManager1` activation service. Before installing that option,
back up current desktop configuration:

```sh
python3 packaging/desktop-integration.py backup
cmake -S . -B build-local -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo -DBUILD_TESTING=ON \
  -DCMAKE_INSTALL_PREFIX="$HOME/.local" \
  -DORCA_INSTALL_DEFAULT_FILE_MANAGER=ON
cmake --build build-local
cmake --install build-local
orca-desktop-integration enable
```

The desktop launcher is `io.github.colozha.OrcaExplorer.desktop`. The helper sets
`inode/directory`, enables the user FileManager1 service, and selects Orca for the
FileChooser portal. Other portal interfaces retain their desktop backends. Empty
GNOME Home shortcut bindings receive Super+E; existing bindings are preserved.
Native Brave Origin and VS Code launchers receive `GTK_USE_PORTAL=1`. Restart
already-running applications to apply their launch environment. Flatpak
applications that use the portal need no override.

The opt-in daemon runs `orca-explorer --daemon --file-manager-service`. It claims
`org.freedesktop.FileManager1` without replacing an existing owner. If another
file manager currently owns that name, close that application's background
service before enabling Orca. Ordinary Orca windows keep their own application
bus names. The desktop daemon uses a separate Orca identity so that Orca's
ordinary unique daemon can run alongside it.

## Private dependencies on this computer

The Fedora installation uses `~/.local/opt/orca-deps`, containing the extracted
runtime/development dependencies and a rebuilt Baloo Widgets 26.08.2. CMake
metadata has been relocated from the previous temporary prefix. Build with:

```sh
cmake -S . -B build-local -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo -DBUILD_TESTING=ON \
  -DCMAKE_INSTALL_PREFIX="$HOME/.local" \
  -DCMAKE_PREFIX_PATH="$HOME/.local/opt/orca-deps" \
  -DQt6Multimedia_DIR="$HOME/.local/opt/orca-deps/lib64/cmake/Qt6Multimedia" \
  -DQt6MultimediaWidgets_DIR="$HOME/.local/opt/orca-deps/lib64/cmake/Qt6MultimediaWidgets" \
  -DKDE_INSTALL_LIBDIR=lib64 -DKDE_INSTALL_BINDIR=libexec/orca-explorer \
  -DORCA_RUNTIME_DEPENDENCY_PREFIX="$HOME/.local/opt/orca-deps" \
  -DORCA_INSTALL_DEFAULT_FILE_MANAGER=ON
```

Public launchers live in `~/.local/bin`; executables live in
`~/.local/libexec/orca-explorer`; private libraries live in
`~/.local/lib64/orca-explorer`. The launchers scope library, plugin, and data paths
to Orca and its children. No global `LD_LIBRARY_PATH` or Qt setting is installed.
The installed launcher, desktop entry, D-Bus activation and systemd units use the
same environment. Installation must not require the source/build directory or
`/tmp/orca-deps` at runtime.

## Picker behavior and limitations

The picker reuses Orca's folder view and Places storage. It supports file/folder
selection, multiple selection, glob/MIME filters, suggested locations/names,
extra application choices, keyboard navigation, and cancellation. Save confirms
replacement; SaveFiles generates unused names for collisions. Selection returns
URIs; the caller performs the actual read/write operation.

The backend exposes `OpenFile`, `SaveFile`, and `SaveFiles` on
`org.freedesktop.impl.portal.FileChooser`, at `/org/freedesktop/portal/desktop`,
with bus name `org.freedesktop.impl.portal.desktop.orca`. Only the portal's bus
owner may invoke it. Requests have independent dialogs and support `Request.Close`.
X11 parent IDs and exported Wayland parent handles are passed to KWindowSystem.

Portal selections must resolve to local `file://` URIs. Use a mounted local
network folder in the picker; unmounted KIO URLs are available in Orca's normal
file manager, but are not accepted by this picker. Applications using custom
GTK/Qt dialogs, browser-internal pickers, or VS Code's simple/remote dialogs may
bypass the portal. The portal preference list chooses available implementations;
it does not guarantee recovery from a backend crash.

## Verification and rollback

Run tests in isolated XDG directories and an isolated D-Bus session. GUI tests
require a working window manager; Xvfb alone does not provide reliable focus.
The portal protocol suite creates its own private D-Bus session.

```sh
test_root=$(mktemp -d)
mkdir -p "$test_root"/{config,data,state,cache}
env XDG_CONFIG_HOME="$test_root/config" XDG_DATA_HOME="$test_root/data" \
  XDG_STATE_HOME="$test_root/state" XDG_CACHE_HOME="$test_root/cache" \
  LD_LIBRARY_PATH="$HOME/.local/opt/orca-deps/lib64" \
  XDG_DATA_DIRS="$HOME/.local/opt/orca-deps/share:/usr/local/share:/usr/share" \
  QT_QPA_PLATFORM=xcb \
  dbus-run-session -- ctest --test-dir build-local --output-on-failure -j1
python3 packaging/test_desktop_integration.py
systemctl --user status orca-explorer-filemanager.service orca-explorer-portal.service
xdg-mime query default inode/directory
```

Snapshots live in `~/.local/state/orca-explorer/integration-backups`. Restore the
latest snapshot with:

```sh
orca-desktop-integration restore
# Or select an earlier snapshot:
orca-desktop-integration restore --backup /absolute/path/to/snapshot-directory
```

Rollback restores previous MIME/portal configuration, launchers, shortcut and
optional FileManager1 activation file, and stops Orca's integration services.
The Orca installation remains available. Reinstall with the default-file-manager
option enabled before enabling again after a rollback that removed activation.


## Verification on this computer, 7 October 2026

Installed for `husain` on Fedora 44 GNOME Classic Wayland, with Qt 6.11.2 and
KDE Frameworks 6.30.0. The `RelWithDebInfo` build has testing enabled.

| Check | Result |
| --- | --- |
| Original CTest suites plus portal protocol/dialog suites | 23/23 passed in isolated XDG/D-Bus, Xvfb and Openbox |
| Request isolation, cancellation, caller authorization and FileManager1 ownership conflict | Passed |
| Single/multiple files, directories, glob/MIME filters, extra choices | Passed |
| Unicode/spaces, invalid paths, restricted permissions, overwrite confirmation | Passed |
| Suggested Save name survives folder navigation; picker never writes caller files | Passed |
| Light/dark, keyboard navigation, 100%/200% scale, 520×400 compact window | Passed on Wayland |
| `xdg-open`, GIO, FileManager1 ShowFolders, ShowItems and ShowItemProperties | Passed; ShowItems selects the requested Unicode file |
| GTK 3 native Open/Save | Passed with a real parent window |
| Qt 6 native Open/Save with `QT_QPA_PLATFORMTHEME=xdgdesktopportal` | Passed |
| Brave Origin native and Brave Flatpak upload/download Save | Passed using temporary profiles; file contents checked |
| VS Code native Open/Save As | Passed using a temporary profile with extensions disabled |
| Ordinary Orca daemon and desktop FileManager1 daemon running together | Passed in isolation and the live session |
| REUSE licensing, desktop entries, systemd unit validation | Passed |
| Cold D-Bus activation of both Orca services | Passed, approximately 0.3 seconds each |
| Install manifest and runtime linkage | 407 installed paths, 16 ELF files, 146 translations, 9 icons and 6 plugin paths checked; no build or temporary runtime dependency |
| Backup/restore helper and automatic rollback on failed activation | Passed |

The enabled FileManager1 user service, MIME preference, portal preference, and
Super+E binding are persistent settings. An actual logout/login was not performed,
since it would terminate the current desktop session. Cold activation was tested
instead; after a future login, the status/MIME commands above can verify the new
session.

Qt applications must use their native portal dialog; `DontUseNativeDialog` and
embedded/custom GTK dialogs bypass this integration. The Qt test runs after the
application event loop starts, so the toolkit has retrieved the portal version.
VS Code simple dialogs, remote workspaces, and embedded browser/app dialogs remain
application-controlled. Opening Nautilus or Dolphin explicitly still launches
those applications. Other installed applications, including Android Studio,
DBeaver, Postman, Zed, Telegram and Edge, have not received application-specific
picker verification. Their behavior depends on whether that operation uses the
portal.

Validation logs, protocol results and theme/scale screenshots are retained under
`~/.local/state/orca-explorer/validation/2026-10-07`. The original desktop snapshot
is `~/.local/state/orca-explorer/integration-backups/20261007-215318-590205`.

The backend follows the [FileChooser backend contract](https://flatpak.github.io/xdg-desktop-portal/docs/doc-org.freedesktop.impl.portal.FileChooser.html)
and [portal configuration](https://flatpak.github.io/xdg-desktop-portal/docs/portals.conf.html).
