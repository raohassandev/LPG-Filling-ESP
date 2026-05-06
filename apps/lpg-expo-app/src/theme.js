// Design tokens — central source of truth for colors, spacing, radii, type sizes.
// All UI components import from here. Do not introduce ad-hoc colors elsewhere.

export const C = {
  // Base
  bg:       "#0f1117",
  surface:  "#1a1d27",
  surface2: "#222633",
  border:   "#2a2d3a",
  muted:    "#6b7280",
  text:     "#f0f2f5",
  textSub:  "#9ca3af",

  // Semantic
  ready:    "#10b981",
  warning:  "#f59e0b",
  danger:   "#ef4444",
  active:   "#3b82f6",
  settling: "#8b5cf6",
  complete: "#10b981",
  offline:  "#6b7280",
  sim:      "#dc2626",
  devBg:    "#1c0a0a",

  // UI
  white:    "#ffffff",
  black:    "#000000",
  primary:  "#3b82f6",

  // Legacy aliases retained so existing screens that still import from theme can work
  panel:    "#1a1d27",
  panelAlt: "#222633",
  line:     "#2a2d3a",
  ink:      "#f0f2f5",
  accent:   "#3b82f6",
  ok:       "#10b981",
  okBg:     "rgba(16,185,129,0.15)",
  badBg:    "rgba(239,68,68,0.15)",
  warnBg:   "rgba(245,158,11,0.15)",
  dark:     "#222633",
  soft:     "#222633",
};

export const S = {
  xs: 4, sm: 8, md: 12, lg: 16, xl: 24, xxl: 32,
};

export const R = {
  sm: 6, md: 10, lg: 16, xl: 24, pill: 999,
};

export const T = {
  xs: 11, sm: 13, md: 15, lg: 17, xl: 22, xxl: 32, hero: 48,
};

// Legacy named exports kept for compatibility with utils that may import them
export const colors = C;
export const spacing = { xs: S.xs, sm: S.sm, md: S.md, lg: S.lg, xl: S.xl };
export const radii = { sm: R.sm, md: R.md, pill: R.pill };
