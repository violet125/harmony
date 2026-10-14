import hashlib
from pathlib import Path
import urllib.request
import xml.etree.ElementTree as E
import sys

sys.path.insert(0, str(Path('work/qt-tools').resolve()))
import py7zr

root = E.parse('work/qt-updates.xml')
package = next(e for e in root.findall('PackageUpdate') if e.findtext('Name') == 'qt.qt6.683.win64_msvc2022_64')
archive = package.findtext('DownloadableArchives').split(',')[0].strip()
url = 'https://download.qt.io/online/qtsdkrepository/windows_x86/desktop/qt6_683/qt6_683/' + package.findtext('Name') + '/' + package.findtext('Version') + archive
target = Path('work/qtbase.7z')
print('Downloading Qt base', flush=True)
with urllib.request.urlopen(url, timeout=60) as response, target.open('wb') as output:
    total = 0
    while chunk := response.read(1024 * 1024):
        output.write(chunk)
        total += len(chunk)
        if total % (16 * 1024 * 1024) == 0:
            print(f'{total // (1024*1024)} MiB', flush=True)
expected = urllib.request.urlopen(url + '.sha1', timeout=60).read().decode().split()[0]
assert hashlib.sha1(target.read_bytes()).hexdigest() == expected
print('Checksum verified. Extracting...', flush=True)
with py7zr.SevenZipFile(target) as archive:
    archive.extractall('work/qt')
print('Qt extraction complete', flush=True)
