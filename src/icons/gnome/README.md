<!-- SPDX-FileCopyrightText: 2026 Dolphin Contributors -->
<!-- SPDX-License-Identifier: CC0-1.0 -->

# Adwaita assets for Dolphin

Source: GNOME Adwaita Icon Theme 50.0, as shipped in Fedora's `adwaita-icon-theme-50.0-1.fc44` package. Upstream: https://gitlab.gnome.org/GNOME/adwaita-icon-theme . Copyright: The GNOME Project.

These assets are distributed under the GNU LGPL version 3 (the upstream theme also offers CC BY-SA 3.0 US). The selected LGPL license text is in `LICENSES/LGPL-3.0-only.txt` at the repository root. The REUSE declarations cover the SVG assets. No GNOME application logo is bundled.

`color/` retains the upstream full-color artwork. Six SVGs use uniform opacity masks that Qt Tiny SVG cannot render; those masks are flattened to equivalent group opacity (verified against librsvg at 256px, identical alpha; small rounding differences in masked shadows). Clip definitions are kept inside `<defs>` so Qt does not paint their rectangles as artwork. `light/` and `dark/` contain the upstream symbolic shapes with non-transparent fill/stroke colors changed to `#2e3436` and `#ffffff`, respectively. This keeps Qt rendering independent of GTK's symbolic recoloring. The resource collection aliases KDE icon names to these assets and includes both private theme variants. Controls use symbolic icons; the file view uses exact `:/dolphin/file-icons/` resource matches to retain full-color icons at every zoom level and preserve custom folder names and paths. Qt generates disabled icon variants.

Unmapped icons, application icons, custom folder icons and absolute icon paths remain available through the existing fallback paths. The assets are only selected inside Dolphin's GNOME appearance.
