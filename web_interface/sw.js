const CACHE_NAME = 'heartpatch-cache-v1';
const urlsToCache = [
  './',
  './dashboard.html',
  './manifest.json',
  './icon_192.png',
  './icon_512.png',
  'https://cdn.jsdelivr.net/npm/chart.js',
  'https://cdn.jsdelivr.net/npm/luxon@3.4.3/build/global/luxon.min.js',
  'https://cdn.jsdelivr.net/npm/chartjs-adapter-luxon@1.3.1',
  'https://cdn.jsdelivr.net/npm/bootstrap@5.3.0/dist/css/bootstrap.min.css'
];

self.addEventListener('install', event => {
  event.waitUntil(
    caches.open(CACHE_NAME).then(cache => cache.addAll(urlsToCache))
  );
});

self.addEventListener('fetch', event => {
  event.respondWith(
    caches.match(event.request).then(response => response || fetch(event.request))
  );
});