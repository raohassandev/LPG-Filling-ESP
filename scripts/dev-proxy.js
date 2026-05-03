#!/usr/bin/env node
// Dev proxy — single entry point for browser testing.
//
//   http://localhost:4000/api/*  →  ESP32 device (http://DEVICE_HOST)
//   http://localhost:4000/*      →  Expo dev server (http://localhost:EXPO_PORT)
//
// WebSocket connections (Expo hot-reload) are tunnelled through as-is.
//
// Usage: node scripts/dev-proxy.js [device-ip] [proxy-port] [expo-port]

const http      = require("http");
const net       = require("net");
const { URL }   = require("url");

const DEVICE_HOST = process.argv[2] || "lpg-controller.local";
const PORT        = Number(process.argv[3]) || 4000;
const EXPO_PORT   = Number(process.argv[4]) || 8081;

const CORS_HEADERS = {
  "access-control-allow-origin":  "*",
  "access-control-allow-methods": "GET, POST, OPTIONS",
  "access-control-allow-headers": "*",
};

// ── helpers ──────────────────────────────────────────────────────────────────

function readBody(req) {
  return new Promise((resolve) => {
    const chunks = [];
    req.on("data", (c) => chunks.push(c));
    req.on("end",  () => resolve(Buffer.concat(chunks)));
    req.on("error",() => resolve(Buffer.alloc(0)));
  });
}

function proxyHttp(req, res, targetHost, targetPort, extraResponseHeaders) {
  readBody(req).then((body) => {
    const skip = new Set(["host","connection","transfer-encoding","upgrade","keep-alive"]);
    const headers = {};
    for (const [k, v] of Object.entries(req.headers)) {
      if (!skip.has(k.toLowerCase())) headers[k] = v;
    }
    headers["host"]           = targetHost;
    headers["content-length"] = String(body.length);

    const upstream = http.request(
      { hostname: targetHost, port: targetPort, path: req.url, method: req.method, headers },
      (upRes) => {
        const respChunks = [];
        upRes.on("data", (c) => respChunks.push(c));
        upRes.on("end",  () => {
          const respBody = Buffer.concat(respChunks);
          const respHeaders = { ...upRes.headers, ...(extraResponseHeaders || {}) };
          // Fix content-length after merging headers
          respHeaders["content-length"] = String(respBody.length);
          delete respHeaders["transfer-encoding"];
          if (!res.headersSent) {
            res.writeHead(upRes.statusCode, respHeaders);
            res.end(respBody);
          }
        });
        upRes.on("error", () => { if (!res.headersSent) res.end(); });
      }
    );
    upstream.on("error", () => {
      if (!res.headersSent) {
        res.writeHead(502, { "content-type": "application/json" });
        res.end(JSON.stringify({ ok: false, message: `upstream unreachable (${targetHost}:${targetPort})` }));
      }
    });
    upstream.write(body);
    upstream.end();
  });
}

// ── main server ───────────────────────────────────────────────────────────────

const server = http.createServer((req, res) => {
  const urlPath = (req.url || "/").split("?")[0];

  // CORS preflight
  if (req.method === "OPTIONS") {
    res.writeHead(204, CORS_HEADERS);
    res.end();
    return;
  }

  // /api/* → ESP32 device
  if (urlPath.startsWith("/api/")) {
    proxyHttp(req, res, DEVICE_HOST, 80, CORS_HEADERS);
    return;
  }

  // Everything else → Expo dev server
  proxyHttp(req, res, "127.0.0.1", EXPO_PORT, null);
});

// WebSocket tunnel (Expo hot-reload / HMR)
server.on("upgrade", (req, clientSocket, head) => {
  const upSocket = net.connect(EXPO_PORT, "127.0.0.1", () => {
    upSocket.write(
      `${req.method} ${req.url} HTTP/1.1\r\n` +
      Object.entries(req.headers).map(([k, v]) => `${k}: ${v}`).join("\r\n") +
      "\r\n\r\n"
    );
    upSocket.write(head);
    clientSocket.pipe(upSocket);
    upSocket.pipe(clientSocket);
  });
  upSocket.on("error", () => clientSocket.destroy());
  clientSocket.on("error", () => upSocket.destroy());
});

server.on("error", (err) => console.error("[proxy] error:", err.message));

server.listen(PORT, () => {
  console.log(`\nDev proxy  ->  http://localhost:${PORT}`);
  console.log(`Device     ->  http://${DEVICE_HOST}`);
  console.log(`Expo       ->  http://localhost:${EXPO_PORT}`);
  console.log(`\nOpen: http://localhost:${PORT}\n`);
});
