// 사용자가 고른 원본 파일(010100D5.jar, 또는 그것을 담은 zip)에서 게임 데이터를 꺼낸다.
// 이 게임은 소리를 res.gmf 안에 압축해 두어 미리 바꿀 수 없으므로 실행 중에 바꾸고(smafev.wasm, host/audio.c),
// 글꼴은 공개 글꼴로 빌드 때 만든 것(font.gly)을 쓴다. 그래서 가져올 때는 풀고 판본만 확인한다.
// 원본에서 나온 내용은 이 파일에 없다. 브라우저와 node(시험)에서 함께 쓴다.
(function (root) {
'use strict';

// 지원하는 판: 실행 이미지의 SHA-256 → 판 이름. restored/는 기준 판(010100D5) 주소로 쓰여 있고,
// 다른 판은 native/ver/<AID>.map 대응표로 맞춘다(host/ver.h). 판은 호스트가 실행 이미지 크기로 다시 가른다.
const VERSIONS = {
  'f3b1e2b8aab918d9e567d819c5048f9a36be0acb37847e0171ca790f6850ab2e': '010100D5', // KTF 01.00.00(기준)
  '484e9f3aa0669ba07e02933577e1274aa573f02cdf75df171d613aa61998628a': '010100D2', // KTF 01.00.02
};
// 호스트가 읽는 데이터 파일(native/Makefile의 windows 대상과 같은 목록).
const DATA_FILES = ['dataset.dat', 'map.pmd', 'res.gmf', 'script.bin', 'tileset.pts'];

// ---- zip ----

const u16 = (b, o) => b[o] | b[o + 1] << 8;
const u32 = (b, o) => (b[o] | b[o + 1] << 8 | b[o + 2] << 16 | b[o + 3] << 24) >>> 0;

async function inflate(bytes) {
  const stream = new Blob([bytes]).stream().pipeThrough(new DecompressionStream('deflate-raw'));
  return new Uint8Array(await new Response(stream).arrayBuffer());
}

// 이름 → 내용. 폴더 항목은 뺀다.
async function unzip(bytes) {
  let end = bytes.length - 22;
  while (end >= 0 && u32(bytes, end) !== 0x06054b50) end--;
  if (end < 0) throw new Error('zip이나 jar 파일이 아닙니다.');
  const count = u16(bytes, end + 10);
  let at = u32(bytes, end + 16);
  const files = new Map();
  for (let i = 0; i < count; i++) {
    if (u32(bytes, at) !== 0x02014b50) throw new Error('압축 파일 목록이 깨졌습니다.');
    const method = u16(bytes, at + 10), packed = u32(bytes, at + 20);
    const nameLen = u16(bytes, at + 28), extraLen = u16(bytes, at + 30), commentLen = u16(bytes, at + 32);
    const local = u32(bytes, at + 42);
    const name = new TextDecoder().decode(bytes.subarray(at + 46, at + 46 + nameLen));
    at += 46 + nameLen + extraLen + commentLen;
    if (name.endsWith('/')) continue;
    const start = local + 30 + u16(bytes, local + 26) + u16(bytes, local + 28);
    const body = bytes.subarray(start, start + packed);
    if (method === 0) files.set(name, body.slice());
    else if (method === 8) files.set(name, await inflate(body));
    else throw new Error('풀 수 없는 압축 방식: ' + name);
  }
  return files;
}

async function sha256(bytes) {
  const d = new Uint8Array(await crypto.subtle.digest('SHA-256', bytes));
  return Array.from(d, x => x.toString(16).padStart(2, '0')).join('');
}

const base = name => name.slice(name.lastIndexOf('/') + 1);

// sources: [[파일 이름, 내용]]. jar 하나, jar를 담은 zip, 또는 jar와 big.icon을 함께 골라도 된다.
// 실행 이미지는 client.bin<BSS 크기>로 들어 있고 호스트는 client.bin으로 읽는다.
// 단말기 메뉴 아이콘(big.icon, PNG)이 있으면 돌려주는 Map의 icon에 담는다(설치 아이콘으로 쓴다).
async function extractGame(sources) {
  let jar = null, icon = null;
  for (const [name, bytes] of sources) {
    const lower = name.toLowerCase();
    if (base(lower) === 'big.icon') { icon = bytes; continue; }
    if (lower.endsWith('.jar')) { jar = bytes; continue; }
    if (lower.endsWith('.zip')) {
      const inner = await unzip(bytes);
      for (const [n, b] of inner) {
        if (base(n).toLowerCase() === 'big.icon') icon = icon || b;
        else if (n.toLowerCase().endsWith('.jar')) jar = jar || b;
      }
      if (!jar && [...inner.keys()].some(n => /client\.bin\d+$/.test(n))) jar = bytes; // jar를 zip으로 이름만 바꾼 경우
    }
  }
  if (!jar) throw new Error('원본 jar(010100D5.jar 또는 010100D2.jar)를 찾지 못했습니다.');
  const files = await unzip(jar);
  const image = [...files.keys()].find(n => /^client\.bin\d+$/.test(n));
  if (!image) throw new Error('게임 실행 파일(client.bin)이 들어 있지 않습니다.');
  const hash = await sha256(files.get(image));
  if (!VERSIONS[hash]) throw new Error('지원하는 판본이 아닙니다(KTF 판 010100D5, 010100D2만 됩니다).');
  const game = new Map();
  for (const name of DATA_FILES) {
    if (!files.has(name)) throw new Error('게임 데이터가 빠져 있습니다: ' + name);
    game.set(name, files.get(name));
  }
  game.set('client.bin', files.get(image));
  game.icon = icon;
  game.version = VERSIONS[hash];
  return game;
}

root.ECGameData = { unzip, extractGame, sha256, DATA_FILES, VERSIONS };
if (typeof module !== 'undefined') module.exports = root.ECGameData;
})(typeof globalThis !== 'undefined' ? globalThis : this);
