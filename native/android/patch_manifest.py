# patch_manifest.py <apk> <aapt2>: configChanges에 CONFIG_ASSETS_PATHS(0x80000000)를 더한다.
# aapt2가 이 비공개 플래그를 받지 않아 빌드 뒤에 고친다. 없으면 시스템 오버레이가 바뀔 때 Activity가 재생성되고, SDL은 그것을 종료로 처리한다.
import re, shutil, struct, subprocess, sys, zipfile
apk, aapt2 = sys.argv[1], sys.argv[2]
dump = subprocess.run([aapt2, 'dump', 'xmltree', apk, '--file', 'AndroidManifest.xml'], capture_output=True, text=True, check=True).stdout
old = int(re.search(r'configChanges\(0x0101001f\)=(0x[0-9a-f]+)', dump).group(1), 16)
tmp = apk + '.tmp'
with zipfile.ZipFile(apk) as zi, zipfile.ZipFile(tmp, 'w') as zo:
    for it in zi.infolist():
        data = zi.read(it.filename)
        if it.filename == 'AndroidManifest.xml':
            pat = struct.pack('<I', old)
            assert data.count(pat) == 1, data.count(pat)
            data = data.replace(pat, struct.pack('<I', old | 0x80000000))
        zo.writestr(it, data)
shutil.move(tmp, apk)
print(f'configChanges {old:#x} -> {old | 0x80000000:#x}')
