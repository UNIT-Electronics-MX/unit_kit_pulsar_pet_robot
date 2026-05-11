// Pulsar C6 - Service Worker
const CACHE_NAME = 'pulsar-c6-v1';

// Archivos a cachear para uso offline
const ASSETS = [
  './',
  './index.html',
  './script-pulsar-c6-ble.js',
  './manifest.json'
];

// Instalar: cachear assets estáticos
self.addEventListener('install', event => {
  event.waitUntil(
    caches.open(CACHE_NAME).then(cache => cache.addAll(ASSETS))
  );
  self.skipWaiting();
});

// Activar: limpiar caches viejos
self.addEventListener('activate', event => {
  event.waitUntil(
    caches.keys().then(keys =>
      Promise.all(
        keys.filter(k => k !== CACHE_NAME).map(k => caches.delete(k))
      )
    )
  );
  self.clients.claim();
});

// Fetch: cache-first para assets, network-first para todo lo demás
self.addEventListener('fetch', event => {
  // No interceptar peticiones BLE ni externas (Tailwind CDN, Google Fonts)
  const url = new URL(event.request.url);
  if (url.origin !== self.location.origin) return;

  event.respondWith(
    caches.match(event.request).then(cached => {
      if (cached) return cached;
      return fetch(event.request).then(response => {
        // Solo cachear respuestas válidas de nuestro origen
        if (response && response.status === 200 && response.type === 'basic') {
          const clone = response.clone();
          caches.open(CACHE_NAME).then(cache => cache.put(event.request, clone));
        }
        return response;
      }).catch(() => cached);
    })
  );
});
