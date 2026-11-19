# mkpak.py <빌드 폴더> <출력> <ui 폴더>: 글꼴 표, 사운드폰트, UI, 게임 데이터를 앱이 첫 실행 때 푸는 단일 파일로 묶는다.
# 풀린 폴더 구조가 main.c가 기대하는 경로와 맞아야 한다: game_dir=<data>/game, 글꼴 표는 game_dir/../font.gly,
# UI 문서는 game_dir/../ui, 사운드폰트는 game_dir/../1mgm.sf2.
import os, struct, sys
build, out, ui = sys.argv[1], sys.argv[2], sys.argv[3]
items = [('font.gly', f'{build}/font.gly'), ('1mgm.sf2', f'{build}/1mgm.sf2')]
items += [(f'ui/{n}', f'{ui}/{n}') for n in sorted(os.listdir(ui)) if not n.startswith('.')]
game = f'{build}/game'
for d, _, names in sorted(os.walk(game)):
    rel = os.path.relpath(d, game)
    if rel.startswith('META-INF'):
        continue
    for n in sorted(names):
        if n != 'client.bin1236':  # client.bin의 원래 이름. 같은 내용이라 한 벌만 담는다.
            items.append((os.path.normpath(f'game/{rel}/{n}'), f'{d}/{n}'))
with open(out, 'wb') as w:
    w.write(b'ECPK' + struct.pack('<I', len(items)))
    for name, path in items:
        data = open(path, 'rb').read()
        raw = name.encode()
        w.write(struct.pack('<H', len(raw)) + raw + struct.pack('<I', len(data)) + data)
print(len(items), 'files', os.path.getsize(out), 'bytes')
