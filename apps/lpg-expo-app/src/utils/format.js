// Numeric / display formatters — pure functions, no React.

export function kg(value) {
  return Number(value || 0).toFixed(3);
}

export function money(value) {
  return Number(value || 0).toFixed(2);
}

export function pct(value) {
  return `${Number(value || 0).toFixed(0)}%`;
}

export function duration(seconds) {
  const sec = Number(seconds || 0);
  if (!sec) return "—";
  const d = Math.floor(sec / 86400);
  const h = Math.floor((sec % 86400) / 3600);
  const m = Math.floor((sec % 3600) / 60);
  const s = Math.floor(sec % 60);
  if (d > 0) return `${d}d ${h}h ${m}m`;
  if (h > 0) return `${h}h ${m}m ${s}s`;
  if (m > 0) return `${m}m ${s}s`;
  return `${s}s`;
}

export function titleCase(value) {
  const text = String(value || "");
  return text.charAt(0).toUpperCase() + text.slice(1);
}

export function shortDateTime(epochSeconds) {
  const ts = Number(epochSeconds || 0);
  if (ts < 1000000000) return null;
  return new Date(ts * 1000).toLocaleString(undefined, {
    month: "short", day: "numeric", hour: "2-digit", minute: "2-digit",
  });
}

export function fullDateTime(epochSeconds) {
  const ts = Number(epochSeconds || 0);
  if (ts < 1000000000) return null;
  return new Date(ts * 1000).toLocaleString();
}
