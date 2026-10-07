# Contributing to Orca Explorer

The fork is maintained by colozha. File issues and pull requests against https://github.com/colozha/orca-explorer when that repository is published. Include a concrete problem, reproduction steps and relevant validation. Do not invent maintainer email addresses or redirect upstream contributor identities.

Use existing Qt Widgets and KDE Frameworks APIs. Preserve file management behavior and the Dolphin Classic path. New presentation changes must work with light/dark palettes, keyboard navigation, scaling and translated labels. Keep application storage and installation names separate from Dolphin.

Build with `BUILD_TESTING=ON`; run the affected QtTest suites. Use isolated XDG directories for integration checks, and inspect `install_manifest.txt` before shipping. Test GNOME Wayland header gestures on a real desktop.

Retain upstream copyright and SPDX identifiers. Select GPL-3.0-only for new application code and CC0-1.0 for original artwork/documentation, unless a compatible license is explicitly required. Record imported asset sources and licenses. Do not claim ownership of upstream code. Run `reuse lint` before distributing source.

No commits, tags or publishing are performed automatically by the preparation workflow.
