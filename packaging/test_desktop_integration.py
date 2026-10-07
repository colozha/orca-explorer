# SPDX-FileCopyrightText: 2026 Orca Explorer Contributors
# SPDX-License-Identifier: GPL-2.0-or-later
"""Run with python3 packaging/test_desktop_integration.py; never changes desktop settings."""
import importlib.util
import tempfile
from pathlib import Path
from unittest.mock import patch
import subprocess

spec = importlib.util.spec_from_file_location('integration', Path(__file__).with_name('desktop-integration.py'))
integration = importlib.util.module_from_spec(spec)
spec.loader.exec_module(integration)

with tempfile.TemporaryDirectory() as temporary:
    root = Path(temporary)
    integration.CONFIG, integration.DATA, integration.STATE = root / 'config', root / 'data', root / 'state'
    original = integration.CONFIG / 'mimeapps.list'
    missing = integration.DATA / 'dbus-1/services/org.freedesktop.FileManager1.service'
    symlink = integration.CONFIG / 'service-link'
    original.parent.mkdir(parents=True)
    original.write_text('[Default Applications]\ninode/directory=org.kde.dolphin.desktop\n')
    original.chmod(0o600)
    symlink.symlink_to('/original/unit.service')
    integration.TARGETS = [original, missing, symlink]
    def fake_run(*args):
        if args[:3] == ('gsettings', 'get', 'org.gnome.settings-daemon.plugins.media-keys'): return "['<Super>f']"
        return ''
    with patch.object(integration, 'run', fake_run), patch.object(integration.subprocess, 'run', return_value=subprocess.CompletedProcess([], 1, stdout='disabled\n')):
        snapshot = integration.backup()
        original.write_text('changed')
        missing.parent.mkdir(parents=True); missing.write_text('new activation service')
        symlink.unlink(); symlink.symlink_to('/changed/unit.service')
        integration.restore(snapshot)
        assert 'org.kde.dolphin.desktop' in original.read_text()
        assert original.stat().st_mode & 0o777 == 0o600
        assert not missing.exists()
        assert str(symlink.readlink()) == '/original/unit.service'
    config = integration.CONFIG / 'xdg-desktop-portal/portals.conf'
    config.parent.mkdir(parents=True)
    config.write_text('[preferred]\ndefault=gnome;gtk;\norg.freedesktop.impl.portal.Secret=gnome-keyring;\n')
    integration.portal_preferences()
    saved = config.read_text()
    assert 'FileChooser=orca;gnome;gtk;' in saved
    assert 'Secret=gnome-keyring;' in saved and 'default=gnome;gtk;' in saved
print('PASS: backup/restore of files, permissions, symlinks, and preservation of other portal backends')
