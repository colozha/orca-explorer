#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Orca Explorer Contributors
# SPDX-License-Identifier: GPL-2.0-or-later
"""Back up, enable, or restore Orca desktop integration for the current user."""
import argparse
import base64
import configparser
import datetime
import json
import os
from pathlib import Path
import subprocess

HOME_DIR = Path.home()
CONFIG = Path(os.environ.get('XDG_CONFIG_HOME', HOME_DIR / '.config'))
DATA = Path(os.environ.get('XDG_DATA_HOME', HOME_DIR / '.local/share'))
STATE = Path(os.environ.get('XDG_STATE_HOME', HOME_DIR / '.local/state')) / 'orca-explorer/integration-backups'
TARGETS = [
    CONFIG / 'mimeapps.list', DATA / 'applications/mimeapps.list',
    CONFIG / 'xdg-desktop-portal/portals.conf',
    DATA / 'dbus-1/services/org.freedesktop.FileManager1.service',
    DATA / 'applications/brave-origin.desktop',
    DATA / 'applications/com.microsoft.VSCode.desktop',
    CONFIG / 'systemd/user/graphical-session.target.wants/orca-explorer-filemanager.service',
]

def run(*args):
    return subprocess.run(args, check=True, text=True, capture_output=True).stdout.strip()

def backup():
    directory = STATE / datetime.datetime.now().strftime('%Y%m%d-%H%M%S-%f')
    directory.mkdir(parents=True, mode=0o700)
    saved = []
    for path in TARGETS:
        if path.is_symlink(): record = {'symlink': os.readlink(path)}
        elif path.exists(): record = {'data': base64.b64encode(path.read_bytes()).decode(), 'mode': path.stat().st_mode & 0o777}
        else: record = {'absent': True}
        saved.append({'path': str(path), **record})
    snapshot = {'files': saved, 'home_shortcut': run('gsettings', 'get', 'org.gnome.settings-daemon.plugins.media-keys', 'home'),
                'filemanager_enabled': subprocess.run(['systemctl', '--user', 'is-enabled', 'orca-explorer-filemanager.service'], capture_output=True, text=True).stdout.strip()}
    (directory / 'snapshot.json').write_text(json.dumps(snapshot, indent=2) + '\n')
    STATE.mkdir(parents=True, exist_ok=True)
    (STATE / 'latest').write_text(str(directory) + '\n')
    print(directory)
    return directory

def portal_preferences():
    path = CONFIG / 'xdg-desktop-portal/portals.conf'
    parser = configparser.ConfigParser(interpolation=None, strict=False)
    parser.optionxform = str
    if path.exists(): parser.read(path)
    else: parser.read('/usr/share/xdg-desktop-portal/gnome-portals.conf')
    if not parser.has_section('preferred'): parser.add_section('preferred')
    parser['preferred']['org.freedesktop.impl.portal.FileChooser'] = 'orca;gnome;gtk;'
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open('w') as stream: parser.write(stream, space_around_delimiters=False)

def app_launchers():
    # Native GTK choosers in these installed applications support portal selection.
    for name in ('brave-origin.desktop', 'com.microsoft.VSCode.desktop'):
        source = Path('/usr/share/applications') / name
        destination = DATA / 'applications' / name
        if not source.exists(): continue
        text = destination.read_text() if destination.exists() else source.read_text()
        lines = []
        for line in text.splitlines():
            if line.startswith('Exec=') and 'GTK_USE_PORTAL=1' not in line:
                line = 'Exec=env GTK_USE_PORTAL=1 ' + line[5:]
            lines.append(line)
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_text('\n'.join(lines) + '\n')

def enable():
    marker = STATE / 'latest'
    if not marker.exists(): raise SystemExit('Run backup before installing or enabling desktop integration.')
    service = DATA / 'dbus-1/services/org.freedesktop.FileManager1.service'
    if not service.exists(): raise SystemExit('Install with ORCA_INSTALL_DEFAULT_FILE_MANAGER=ON first.')
    run('systemctl', '--user', 'daemon-reload')
    # Never evict another FileManager1 owner; the new daemon fails clearly on conflict.
    try:
        run('systemctl', '--user', 'enable', '--now', 'orca-explorer-filemanager.service')
        run('xdg-mime', 'default', 'io.github.colozha.OrcaExplorer.desktop', 'inode/directory')
        portal_preferences()
        app_launchers()
        if run('gsettings', 'get', 'org.gnome.settings-daemon.plugins.media-keys', 'home') in ("[]", "@as []", "['']"):
            run('gsettings', 'set', 'org.gnome.settings-daemon.plugins.media-keys', 'home', "['<Super>e']")
        run('update-desktop-database', str(DATA / 'applications'))
        run('systemctl', '--user', 'restart', 'xdg-desktop-portal.service')
        run('gdbus', 'call', '--session', '--dest', 'org.freedesktop.DBus', '--object-path', '/org/freedesktop/DBus',
            '--method', 'org.freedesktop.DBus.StartServiceByName', 'org.freedesktop.impl.portal.desktop.orca', '0')
    except (subprocess.CalledProcessError, OSError):
        restore(Path(marker.read_text().strip()))
        raise
    print('Orca desktop integration enabled. Restart running applications to use the updated launchers.')

def restore(directory):
    saved = json.loads((directory / 'snapshot.json').read_text())
    subprocess.run(['systemctl', '--user', 'disable', '--now', 'orca-explorer-filemanager.service'], capture_output=True)
    subprocess.run(['systemctl', '--user', 'stop', 'orca-explorer-portal.service'], capture_output=True)
    for record in saved['files']:
        path = Path(record['path'])
        if path.is_symlink() or path.exists(): path.unlink()
        if 'symlink' in record:
            path.parent.mkdir(parents=True, exist_ok=True); path.symlink_to(record['symlink'])
        elif 'data' in record:
            path.parent.mkdir(parents=True, exist_ok=True); path.write_bytes(base64.b64decode(record['data'])); path.chmod(record['mode'])
    run('gsettings', 'set', 'org.gnome.settings-daemon.plugins.media-keys', 'home', saved['home_shortcut'])
    run('systemctl', '--user', 'daemon-reload')
    if saved['filemanager_enabled'] == 'enabled': run('systemctl', '--user', 'enable', '--now', 'orca-explorer-filemanager.service')
    run('update-desktop-database', str(DATA / 'applications'))
    run('systemctl', '--user', 'restart', 'xdg-desktop-portal.service')
    print('Restored desktop settings from', directory)

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=('backup', 'enable', 'restore'))
    parser.add_argument('--backup', type=Path)
    args = parser.parse_args()
    if args.action == 'backup': backup()
    elif args.action == 'enable': enable()
    else: restore(args.backup or Path((STATE / 'latest').read_text().strip()))
