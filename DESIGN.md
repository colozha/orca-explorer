# Dolphin GNOME appearance

This is an optional presentation of Dolphin for everyday file management. The visual reference is the user's GNOME Files 50.3 screenshots: a dark content surface, slightly lighter sidebar, flat symbolic controls, a rounded breadcrumb, spacious file grid, and blue Adwaita folders. Light mode uses the corresponding Adwaita palette. Dolphin retains its name, logo, actions, shortcuts, backend, and settings.

The running appearance is selected once at startup. Standard Dolphin remains the default. GNOME mode uses a Fusion proxy style, application-local palettes and resource icon themes, a separate XMLGUI toolbar definition and window-state group initialized from the existing dock layout, and a thin header around the existing toolbar. Desktop configuration is never written. Explicit Window Color Scheme choices and saved view sizes take precedence over appearance defaults.

GNOME Wayland gets application-owned window controls and system move/resize operations. Other platforms, floating toolbars, and non-top toolbar positions retain native decoration. The sidebar stays resizable and dock panels stay movable. When the sidebar is hidden or moved, its header collapses to the menu control. Menus, dialogs and file operations use existing Qt/KDE widgets. About links to the complete KDE credits/legal dialog rather than duplicating its metadata.

Icons are a subset of Fedora's `adwaita-icon-theme-50.0-1.fc44` (GNOME Project, LGPL-3.0-only). Full-color artwork is preserved with uniform SVG opacity masks flattened for Qt compatibility; symbolic paths are exported in light and dark foreground colors for Qt's icon loader. Resource aliases map KDE action names to GNOME equivalents. Missing application-specific icons use the user's theme and Breeze fallback. Attribution and license information are in `src/icons/gnome/README.md` and `LICENSES/LGPL-3.0-only.txt`.

Acceptance: native Dolphin passes its existing regression tests; both GNOME palettes have readable controls and intact keyboard navigation; no required action disappears at narrow widths; folders, thumbnails, custom icons, emblems, hidden/cut states, split view and all panels keep their existing behavior. Review at 1920×1033 and 1024×768, 100% and 200% scaling, with long names and multiple tabs. Compositor shadows, native portal dialogs, fonts and user-selected palettes may differ from the screenshots.

Implementation references: [KMainWindow state persistence](https://api.kde.org/kmainwindow.html), [QWindow system move/resize](https://doc.qt.io/qt-6/qwindow.html), and [Adwaita source and licenses](https://gitlab.gnome.org/GNOME/adwaita-icon-theme). No additional Qt private API or runtime dependency is introduced.

## Validation

Verified on Fedora 44 with Qt 6.11.2 and KDE Frameworks 6.30. The minimum Qt requirement remains 6.4; that version was not available for a separate build. The build includes Baloo and the Information panel.

The 15 targeted CTest suites pass: main window, Places, Dolphin/KFileItem views, selection manager, item controllers/expansion, drag-and-drop, view properties, query/search bar, smoke, both appearance startup paths, and the existing wait-style check. The GNOME appearance test also passes directly on GNOME Wayland (8 checks). It covers settings persistence/reset, saved icon sizes, palette/icon changes and fallback, rounded breadcrumb painting, Back/Forward geometry, split navigators, toolbar relocation/customization, retained menu actions, close confirmation, and migration of an existing dock layout. The actual executable's GNOME startup was smoke-tested with temporary configuration.

Final light/dark captures cover the main window, split/tabs/filter, About, Interface preferences and Properties, including long names, hidden files, a thumbnail, a symlink, a custom folder icon and a read-only file/location. GNOME Wayland was checked at 100%; 200% used a larger X11 virtual screen to retain the requested logical window sizes. Compact captures are 1024×768; the GNOME compositor maximizes the reference-sized window to its 1920×1034 work area. X11 captures retain 1920×1033. Native GNOME maximize/restore/minimize and close confirmation passed.

Remaining manual validation: real-pointer dragging, resizing from all edges/corners, snapping, and compositor interaction across multiple GNOME Wayland windows. Synthetic Qt input cannot supply the compositor's pointer serial for system move/resize. Long translated labels and a separate build on minimum Qt also need their target environments. Portal/native dialogs, compositor shadows and external applications follow the system.

Enable through Settings → Configure Dolphin → Interface → Appearance → GNOME Files, then restart Dolphin. Returning to Dolphin uses the original toolbar definition and native state group. No desktop settings, file associations or backend services are modified.


## Orca Explorer identity — 2026-10-07

The independent fork is Orca Explorer 0.1.0, based on Dolphin 26.08.2.
Fresh configurations default to GNOME Files; Dolphin Classic remains available.
All presentation resources and settings remain process-local. New artwork is
an original CC0 orca silhouette on a blue folder; upstream artwork/credits stay
in source. See docs/PROVENANCE.md and docs/DISTRIBUTION.md for licensing,
storage separation and installation contracts.

KFilePlacesModel currently hard-codes the generic XDG data bookmark path and
has no public alternate-file constructor. Its synchronous initial construction
uses a scoped XDG_DATA_HOME override to the Orca application data directory,
restoring the environment immediately afterwards. The model retains its own
bookmark manager thereafter. This must remain an early, single-threaded model
initialization before background workers are started; tests verify restoration
and separation. No global environment or desktop configuration is changed.
