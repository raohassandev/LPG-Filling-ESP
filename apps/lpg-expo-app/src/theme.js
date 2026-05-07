// Design tokens — central source of truth for colors, spacing, radii, type sizes.
// All UI components import from here. Do not introduce ad-hoc colors elsewhere.
// Color choices: dark base (good for indoor fill stations + night), semantic
// colors boosted ~15% brightness vs pure Tailwind defaults for outdoor readability.

export const C = {
  // Base — dark industrial palette
  bg:       "#0d1018",   // near-black background
  surface:  "#161b26",   // card/panel surface
  surface2: "#1e2535",   // secondary/input surface
  border:   "#2b3045",   // subtle borders
  muted:    "#5c6880",   // muted text / disabled
  text:     "#f2f5fa",   // primary text (high contrast)
  textSub:  "#8a95aa",   // secondary text

  // Semantic — slightly brighter than Tailwind defaults for outdoor screen visibility
  ready:    "#1ecc94",   // vivid green — good / safe / complete
  warning:  "#fbbf35",   // vivid amber — caution / slow fill
  danger:   "#f85252",   // vivid red — fault / e-stop / danger
  active:   "#4f9eff",   // vivid blue — filling / selected / primary action
  settling: "#9d70f5",   // purple — settling phase
  complete: "#1ecc94",   // same as ready
  offline:  "#5c6880",   // gray — disconnected
  sim:      "#e52222",   // dark red — simulation mode warning

  // UI
  white:    "#ffffff",
  black:    "#000000",
  primary:  "#4f9eff",

  // Legacy aliases
  panel:    "#161b26",
  panelAlt: "#1e2535",
  line:     "#2b3045",
  ink:      "#f2f5fa",
  accent:   "#4f9eff",
  ok:       "#1ecc94",
  okBg:     "rgba(30,204,148,0.12)",
  badBg:    "rgba(248,82,82,0.12)",
  warnBg:   "rgba(251,191,53,0.12)",
  dark:     "#1e2535",
  soft:     "#1e2535",
  devBg:    "#1c0a0a",
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
