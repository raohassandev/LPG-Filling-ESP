export function kg(value) {
  return Number(value || 0).toFixed(3);
}

export function money(value) {
  return Number(value || 0).toFixed(2);
}

export function titleCase(value) {
  const text = String(value || "");
  return text.charAt(0).toUpperCase() + text.slice(1);
}
