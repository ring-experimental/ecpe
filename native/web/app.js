// 페이지 쪽 진행: 저장소 연결, 원본 파일 가져오기, 게임 시작, 세이브 파일 주고받기. 구조는 크로노스 윙 웹(native/web/app.js)과 같다.
// 저장소(/persist, IndexedDB): game/ 원본에서 푼 데이터, icons/ 설치 아이콘, save/ 세이브와 ui.ini 설정.
'use strict';
const $ = id => document.getElementById(id);
const PERSIST = '/persist', DATA_VERSION = '1', IMPORT_PATH = '/tmp/import.ecsave';
let store;

function status(text, error) {
  $('status').textContent = text;
  $('status').classList.toggle('error', !!error);
}

var Module = {
  noInitialRun: true,
  canvas: $('canvas'),
  printErr: text => console.warn(text),
  onRuntimeInitialized: boot,
  onAbort: what => { $('setup').hidden = false; $('need').hidden = $('ready').hidden = true; status('오류로 멈췄습니다: ' + what, true); },
};

// 바뀐 파일만 IndexedDB에 쓴다. 쓰는 중에 또 부르면 끝난 뒤 한 번 더 돈다.
let syncing = false, again = false;
Module.saveSync = () => {
  if (!store) return;
  if (syncing) { again = true; return; }
  syncing = true;
  store.syncfs(false, err => {
    if (err) console.warn('저장소 쓰기 실패', err);
    syncing = false;
    if (again) { again = false; Module.saveSync(); }
  });
};
const syncNow = () => new Promise(done => store.syncfs(false, done));

const exists = path => store.analyzePath(path).exists;
const dataReady = () => exists(PERSIST + '/game/client.bin') && ECGameData.DATA_FILES.every(n => exists(PERSIST + '/game/' + n))
  && exists(PERSIST + '/data.version') && store.readFile(PERSIST + '/data.version', { encoding: 'utf8' }) === DATA_VERSION;

// 터치 화면과 설치 여부. 설치한 앱(매니페스트 display: fullscreen)은 이미 전체 화면이다.
const TOUCH = matchMedia('(pointer: coarse)').matches || navigator.maxTouchPoints > 0;
const IOS = /iPad|iPhone|iPod/.test(navigator.userAgent) || (navigator.platform === 'MacIntel' && navigator.maxTouchPoints > 1);
const installed = () => navigator.standalone === true || matchMedia('(display-mode: fullscreen), (display-mode: standalone)').matches;

function show() {
  const ok = dataReady();
  $('need').hidden = ok;
  $('ready').hidden = !ok;
  $('ios-tip').hidden = !(IOS && !installed());
  $('install-tip').hidden = IOS || installed();
  status('');
}

// ---- 설치 아이콘 ----
// 원본의 단말기 아이콘(big.icon, PNG)을 설치한 앱의 아이콘으로 쓴다. 이 기기의 저장소에만 두며 배포 파일에는 임시 아이콘만 있다.
// 서비스 워커가 저장소에서 꺼내 icon-*.png 요청에 돌려준다(sw.js). 주소는 http(s) 그대로여야 Chrome이 앱으로 설치해 준다.
const ICONS = PERSIST + '/icons', ICON_SIZES = [512, 192, 180];

async function saveIcons(png) {
  const image = await createImageBitmap(new Blob([png], { type: 'image/png' }));
  const square = (size, smooth) => {
    const c = document.createElement('canvas');
    c.width = c.height = size;
    const g = c.getContext('2d');
    g.imageSmoothingEnabled = smooth;
    return [c, g];
  };
  // 도트가 뭉개지지 않게 정수배로 키워 가운데에 놓는다. 둘레는 그림의 왼쪽 위 색으로 채워 둥근 마스크에 잘려도 되게 한다(가운데 80% 안).
  const [big, g] = square(512, false);
  const k = Math.max(1, Math.floor(512 * 0.8 / Math.max(image.width, image.height)));
  const probe = document.createElement('canvas');
  probe.width = probe.height = 1;
  const pg = probe.getContext('2d');
  pg.drawImage(image, 0, 0);
  const [r, gr, b, a] = pg.getImageData(0, 0, 1, 1).data;
  g.fillStyle = a === 255 ? `rgb(${r},${gr},${b})` : '#0b0d12';
  g.fillRect(0, 0, 512, 512);
  g.drawImage(image, (512 - image.width * k) >> 1, (512 - image.height * k) >> 1, image.width * k, image.height * k);
  store.mkdirTree(ICONS);
  for (const size of ICON_SIZES) {
    let canvas = big;
    if (size !== 512) {
      const [small, sg] = square(size, true);
      sg.imageSmoothingQuality = 'high';
      sg.drawImage(big, 0, 0, size, size);
      canvas = small;
    }
    const blob = await new Promise(done => canvas.toBlob(done, 'image/png'));
    store.writeFile(`${ICONS}/icon-${size}.png`, new Uint8Array(await blob.arrayBuffer()));
  }
  store.writeFile(ICONS + '/hash', (await ECGameData.sha256(png)).slice(0, 12));
}

// 저장된 아이콘이 있으면 홈 화면 아이콘과 탭 아이콘을 바꾸고, 서비스 워커가 페이지를 맡은 뒤 매니페스트 주소에 표시를 붙인다.
// 주소가 바뀌어야 브라우저가 매니페스트와 아이콘을 다시 읽는다. 서비스 워커가 없으면 서버가 표시를 무시해 임시 아이콘이 쓰인다.
async function applyIcons() {
  if (!exists(ICONS + '/hash')) return;
  const hash = store.readFile(ICONS + '/hash', { encoding: 'utf8' });
  const url = size => URL.createObjectURL(new Blob([store.readFile(`${ICONS}/icon-${size}.png`)], { type: 'image/png' }));
  const data = size => new Promise(done => { const r = new FileReader(); r.onload = () => done(r.result); r.readAsDataURL(new Blob([store.readFile(`${ICONS}/icon-${size}.png`)], { type: 'image/png' })); });
  $('touch-icon').href = await data(180); // iOS는 홈 화면에 추가하는 순간의 이 주소를 쓴다.
  $('favicon').type = 'image/png';
  $('favicon').href = url(192);
  if (!('serviceWorker' in navigator)) return;
  const controlled = navigator.serviceWorker.controller ? Promise.resolve() : new Promise(done => {
    navigator.serviceWorker.addEventListener('controllerchange', done, { once: true });
    setTimeout(done, 10000);
  });
  await controlled;
  if (navigator.serviceWorker.controller) $('manifest').href = 'manifest.webmanifest?orig=' + hash;
}

function boot() {
  store = Module.FS;
  store.mkdir(PERSIST);
  store.mount(Module.IDBFS, {}, PERSIST);
  store.syncfs(true, err => {
    if (err) { status('브라우저 저장소를 열지 못했습니다. 사생활 보호 창에서는 실행할 수 없습니다.', true); return; }
    show();
    applyIcons();
  });
}

function removeTree(path) {
  if (!exists(path)) return;
  for (const name of store.readdir(path)) {
    if (name === '.' || name === '..') continue;
    const child = path + '/' + name;
    if (store.isDir(store.stat(child).mode)) removeTree(child); else store.unlink(child);
  }
  store.rmdir(path);
}

async function install(files) {
  $('need').hidden = true;
  try {
    status('원본 파일을 푸는 중…');
    const sources = [];
    for (const f of files) sources.push([f.name, new Uint8Array(await f.arrayBuffer())]);
    const game = await ECGameData.extractGame(sources);
    status('저장하는 중…');
    removeTree(PERSIST + '/game');
    for (const [name, data] of game) {
      const path = PERSIST + '/game/' + name;
      store.mkdirTree(path.slice(0, path.lastIndexOf('/')));
      store.writeFile(path, data);
    }
    store.writeFile(PERSIST + '/data.version', DATA_VERSION);
    store.writeFile(PERSIST + '/game.version', game.version);
    removeTree(ICONS);
    if (game.icon) await saveIcons(game.icon).catch(e => console.warn('아이콘을 만들지 못함', e));
    const err = await syncNow();
    if (err) throw new Error('브라우저 저장소에 쓰지 못했습니다. 남은 공간을 확인하세요.');
    show();
    applyIcons();
  } catch (e) {
    console.error(e);
    show();
    status(e.message || String(e), true);
  }
}

// ---- 전체화면 ----
// 폰 브라우저는 주소창 때문에 화면이 작아진다. 터치 기기는 시작할 때 전체화면으로 들어가 가로로 고정하고, 뒤로 제스처로 빠져나오면
// 다음 터치 때 다시 들어간다. 설치한 앱은 이미 전체 화면이라 요청하지 않는다(요청하면 Chrome이 종료 안내를 띄운다).
// 이 건너뛰기는 터치 화면에만 둔다. 데스크톱 브라우저의 F11 상태도 display-mode: fullscreen으로 잡히기 때문이다.
// 브라우저는 사용자 입력을 처리하는 동안에만 허락하므로 버튼, 터치, 키 처리에서 바로 부른다. iPhone의 Safari처럼 지원하지 않으면 넘어간다.
const fullscreenOn = () => !!(document.fullscreenElement || document.webkitFullscreenElement);
async function enterFullscreen() {
  const el = document.documentElement, request = el.requestFullscreen || el.webkitRequestFullscreen;
  if (!request || fullscreenOn()) return false;
  try {
    await request.call(el, { navigationUI: 'hide' });
    if (screen.orientation && screen.orientation.lock) screen.orientation.lock('landscape').catch(() => {});
    return true;
  } catch (e) { return false; }
}
const wantsFullscreen = () => TOUCH && !installed();
// 게임 화면 폭은 시작할 때의 페이지 비율로 정해진다. 전체화면과 가로 돌리기가 페이지 크기에 반영될 때까지 잠깐 기다린다.
function settled(ms) {
  return new Promise(done => {
    const t0 = performance.now(), w = innerWidth, h = innerHeight;
    (function wait() {
      if (((innerWidth !== w || innerHeight !== h) && innerWidth > innerHeight) || performance.now() - t0 > ms) done(); else requestAnimationFrame(wait);
    })();
  });
}
let playing = false;
for (const type of ['touchend', 'mouseup']) addEventListener(type, () => { if (playing && wantsFullscreen()) enterFullscreen(); }, true);
// 데스크톱의 F11은 게임이 키를 가져가 브라우저에 닿지 않는다. 페이지가 직접 바꾼다.
addEventListener('keydown', e => {
  if (e.key !== 'F11') return;
  e.preventDefault();
  e.stopPropagation();
  if (fullscreenOn()) (document.exitFullscreen || document.webkitExitFullscreen).call(document); else enterFullscreen();
}, true);

async function start() {
  if (wantsFullscreen() && await enterFullscreen()) await settled(700);
  playing = true;
  $('setup').hidden = true;
  document.body.classList.add('playing');
  if (navigator.storage && navigator.storage.persist) navigator.storage.persist();
  // 소리 변환기(smafev.wasm)는 게임이 실행 중에 부르므로 먼저 열어 둔다(host/audio.c).
  if (!Module.smafev) {
    try { Module.smafev = (await WebAssembly.instantiate(await (await fetch('smafev.wasm')).arrayBuffer(), {})).instance.exports; }
    catch (e) { console.warn('소리 변환기를 열지 못함: 소리 없이 진행', e); }
  }
  // 판마다 세이브 폴더를 나눈다(판끼리 세이브 구조가 같은지 확인하지 않았다). 기준 판은 예전 자리 그대로.
  const version = exists(PERSIST + '/game.version') ? store.readFile(PERSIST + '/game.version', { encoding: 'utf8' }) : '010100D5';
  const saveDir = PERSIST + (version === '010100D5' ? '/save' : '/save-' + version);
  const args = [PERSIST + '/game', '--save', saveDir, '--soundfont', '/1mgm.sf2'];
  const extra = new URLSearchParams(location.search); // 시험용: ?touch=1 화면 패드, ?width=<폭>
  if (navigator.maxTouchPoints > 0 || extra.get('touch')) args.push('--touch');
  if (extra.get('width')) args.push('--width', extra.get('width'));
  $('canvas').focus();
  // SDL은 창을 만들 때 전체화면 창이 아니라며 페이지의 전체화면을 푼다. 전체화면은 이 페이지가 맡으므로 시작하는 동안만 그 호출을 막는다.
  const exits = ['exitFullscreen', 'webkitExitFullscreen'].filter(n => document[n]);
  for (const n of exits) document[n] = () => Promise.resolve();
  try { Module.callMain(args); } finally { for (const n of exits) delete document[n]; }
  // SDL은 페이지를 채우는 창을 만들 때 캔버스가 아닌 것을 모두 숨긴 상자로 옮긴다. 게임 위에 떠야 하는 화면들을 다시 꺼내 놓는다.
  for (const id of ['setup', 'rotate', 'picker', 'ended']) document.body.appendChild($(id));
  setTimeout(focusGame, 0);
}

// 게임은 캔버스에 포커스가 있을 때만 키를 받는다. 입력 칸을 쓰는 중이 아니면 포커스를 캔버스로 돌려 둔다.
function focusGame() {
  if (playing && $('picker').hidden && document.activeElement !== $('canvas')) $('canvas').focus({ preventScroll: true });
}
for (const type of ['pointerdown', 'keydown', 'focusout']) addEventListener(type, () => setTimeout(focusGame, 0), true);

// 게임이 부른다(host/savefile.c).
Module.pickSave = () => { $('savefile').value = ''; $('picker').hidden = false; };
function picked(path) {
  $('picker').hidden = true;
  $('canvas').focus();
  Module.ccall('web_save_picked', null, ['string'], [path]);
}
Module.downloadFile = path => {
  if (!exists(path)) return;
  const url = URL.createObjectURL(new Blob([store.readFile(path)], { type: 'application/octet-stream' }));
  const a = document.createElement('a');
  a.href = url;
  a.download = path.slice(path.lastIndexOf('/') + 1);
  document.body.appendChild(a);
  a.click();
  a.remove();
  setTimeout(() => URL.revokeObjectURL(url), 60000);
  store.unlink(path);
};
Module.onGameExit = () => { $('ended').hidden = false; };

$('source').addEventListener('change', e => { if (e.target.files.length) install([...e.target.files]); });
$('start').addEventListener('click', () => start());
$('forget').addEventListener('click', async () => {
  removeTree(PERSIST + '/game');
  for (const f of ['/data.version', '/game.version']) if (exists(PERSIST + f)) store.unlink(PERSIST + f);
  removeTree(ICONS);
  await syncNow();
  show();
});
$('savefile').addEventListener('change', async e => {
  const file = e.target.files[0];
  if (!file) return;
  store.writeFile(IMPORT_PATH, new Uint8Array(await file.arrayBuffer()));
  picked(IMPORT_PATH);
});
$('cancel').addEventListener('click', () => picked(''));
$('again').addEventListener('click', () => location.reload());
document.addEventListener('visibilitychange', () => { if (document.hidden) Module.saveSync(); });
addEventListener('pagehide', () => Module.saveSync());
addEventListener('contextmenu', e => e.preventDefault());

if ('serviceWorker' in navigator && (location.protocol === 'https:' || location.hostname === 'localhost')) navigator.serviceWorker.register('sw.js');
