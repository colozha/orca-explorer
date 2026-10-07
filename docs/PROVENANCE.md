# Provenance and licensing

Orca Explorer 0.1.0 is an independent fork maintained by colozha. The upstream release baseline is **Dolphin 26.08.2**, from https://github.com/KDE/dolphin.git (canonical development: https://invent.kde.org/system/dolphin). The upstream source revision is `44902d086cf3a795c680887524938f1b2caf3c93`. Orca Explorer starts with a fresh initial commit rather than including Dolphin Git history. Upstream authors and credits are retained; upstream history is available from the Dolphin repositories. The original local source directory is retained separately.

Fork preparation on **2026-10-07** includes the GNOME Files presentation enhancement, application rebranding, isolated storage and integration identities, telemetry removal and original artwork. The upstream version remains the baseline for matching dependencies and VCS ABI; it is not the fork release version.

The combined program is GPL-3.0-only. SPDX declarations on inherited files are not changed. COPYRIGHT ownership is not transferred. The original COPYING, COPYING.DOC and LICENSES directory remain part of every source distribution. The full upstream contributor list is still available in About; historical contributions can be consulted in the upstream Dolphin repositories.

## Bundled assets

- `src/icons/gnome/`: selected Adwaita icons from the GNOME Project, retaining LGPL-3.0-only attribution and the existing asset metadata. Source: https://gitlab.gnome.org/GNOME/adwaita-icon-theme. Licensing reference: https://github.com/GNOME/adwaita-icon-theme/blob/master/COPYING. Files adjusted for Qt SVG rendering remain credited to GNOME; the changes are described in DESIGN.md.
- `src/icons/orca/`: original blue-folder/black-and-white-orca SVG by colozha, created 2026-10-07, with PNG exports at 16, 22, 32, 48, 64, 128, 256 and 512 pixels. CC0-1.0. It is not derived from Dolphin or the GNOME Orca logo.
- Inherited Dolphin artwork is retained in source for historical completeness, with original licensing. The installed application icon is the Orca artwork.
- Upstream translations retain translator credits and licenses. Catalog filenames are changed to avoid collisions. New/changed strings may fall back to English until fork translations are updated.

REUSE.toml and inline SPDX notices provide machine-readable declarations. New fork documentation and artwork are CC0-1.0. See LICENSE for the distinction between the combined distribution and per-file licenses.

The translated handbooks and inherited screenshots remain upstream Dolphin references in private installation paths. The English handbook identifies the fork and redirects its support links. Current fork instructions are in README.md. COPYING.DOC and the original handbook legal notices remain applicable to inherited documentation.
