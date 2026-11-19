// 앱 껍데기를 캐시해 한 번 연 뒤에는 오프라인으로도 열리게 한다. 네트워크를 먼저 쓰므로 새 빌드는 바로 적용된다.
// 게임 데이터와 세이브는 IndexedDB(/persist)에 있고 이 캐시에 넣지 않는다.
// 설치 아이콘: 사용자가 원본의 big.icon을 넣었으면 그 아이콘이 /persist/icons에 있다(app.js). icon-*.png 요청에 그것을 돌려주고,
// manifest.webmanifest?orig=<해시> 요청에는 아이콘 주소에 같은 표시를 붙인 매니페스트를 돌려준다. 주소는 모두 http(s) 그대로다.
const CACHE = 'ecpe-__BUILD__';
const FILES = ['./', 'index.html', 'app.js', 'gamedata.js', 'ecpe.js', 'ecpe.wasm', 'ecpe.data', 'smafev.wasm', 'manifest.webmanifest', 'icon.svg', 'icon-192.png', 'icon-512.png'];
const STORE_DB = '/persist', STORE_NAME = 'FILE_DATA', ICON_DIR = '/persist/icons/';

self.addEventListener('install', e => e.waitUntil(caches.open(CACHE).then(c => c.addAll(FILES)).catch(() => {}).then(() => self.skipWaiting())));
self.addEventListener('activate', e => e.waitUntil(
  caches.keys().then(keys => Promise.all(keys.filter(k => k !== CACHE).map(k => caches.delete(k)))).then(() => self.clients.claim())));

async function networkFirst(request) {
  try {
    // 브라우저의 HTTP 캐시를 그대로 쓰면 새 페이지에 옛 wasm이 섞일 수 있다. 서버에 바뀌었는지 물어보고 받는다.
    const response = await fetch(request.url, { cache: 'no-cache' });
    if (response.ok) (await caches.open(CACHE)).put(request, response.clone());
    return response;
  } catch (e) {
    const hit = await caches.match(request, { ignoreSearch: true });
    if (hit) return hit;
    throw e;
  }
}

// Emscripten IDBFS가 쓰는 저장소에서 파일 하나를 읽는다. 저장소가 아직 없으면 만들지 않고 null을 돌려준다.
function stored(path) {
  return new Promise(done => {
    const open = indexedDB.open(STORE_DB);
    open.onupgradeneeded = () => open.transaction.abort();
    open.onerror = () => done(null);
    open.onsuccess = () => {
      const db = open.result;
      if (!db.objectStoreNames.contains(STORE_NAME)) { db.close(); done(null); return; }
      const get = db.transaction(STORE_NAME).objectStore(STORE_NAME).get(path);
      get.onsuccess = () => { db.close(); done(get.result && get.result.contents ? get.result.contents : null); };
      get.onerror = () => { db.close(); done(null); };
    };
  });
}

async function icon(request, name) {
  const data = await stored(ICON_DIR + name);
  return data ? new Response(data, { headers: { 'Content-Type': 'image/png', 'Cache-Control': 'no-store' } }) : networkFirst(request);
}

async function manifest(request, mark) {
  const base = await networkFirst(new Request(new URL('manifest.webmanifest', self.registration.scope)));
  if (!base.ok || !(await stored(ICON_DIR + 'icon-512.png'))) return base;
  const m = await base.json();
  m.icons = m.icons.filter(i => i.type === 'image/png').map(i => ({ ...i, src: i.src + '?orig=' + encodeURIComponent(mark) }));
  return new Response(JSON.stringify(m), { headers: { 'Content-Type': 'application/manifest+json', 'Cache-Control': 'no-store' } });
}

self.addEventListener('fetch', e => {
  if (e.request.method !== 'GET') return;
  const url = new URL(e.request.url);
  if (url.origin !== location.origin) return;
  const name = url.pathname.slice(url.pathname.lastIndexOf('/') + 1);
  if (/^icon-(192|512)\.png$/.test(name)) e.respondWith(icon(e.request, name));
  else if (name === 'manifest.webmanifest' && url.searchParams.get('orig')) e.respondWith(manifest(e.request, url.searchParams.get('orig')));
  else e.respondWith(networkFirst(e.request));
});
