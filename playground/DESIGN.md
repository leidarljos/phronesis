---
version: alpha
name: phronesis-playground
description: >
  Forensic policy playground (regex101-class) for phronesis. Dark, product-tool
  chrome: near-black Linear canvas, Raycast-style semantic status colors for
  DENY/ALLOW/PROMPT, Warp/Resend monospace for argv and Cap'n traces, Mintlify-like
  encyclopedia density for PolicyReason docs. One lavender accent for focus and
  primary actions; never decorative rainbow chrome.
provenance:
  - https://github.com/VoltAgent/awesome-design-md (Linear, Raycast, Sentry, Warp, Resend, Mintlify, VoltAgent)
  - product: Cap'n PolicyDecision + TraceEvent forensic UX
---

# phronesis playground DESIGN.md

## 1. Visual Theme & Atmosphere

**Mood:** security-tool precision, not marketing. Feels like a dark IDE pane that
happens to explain policy law — continuous near-black surface, hairline panels,
dense but scannable forensic steps.

**Density:** high in `/play` (three panes, suite strip, trace list). Medium on
landing / encyclopedia (Mintlify-style reading band).

**Philosophy:**

- One continuous dark mode (Raycast tonal continuity).
- Chromatic accent is **scarce** (Linear lavender) — reserved for focus rings,
  primary CTA, active nav.
- Semantic color only for **decision outcomes** (Resend/Raycast): red deny,
  green allow, amber prompt — never for decoration.
- Argv, paths, Cap'n codes, and TRACE names live in **mono** (Warp terminal).
- Fail-closed chrome: empty states quiet; errors loud but contained.

## 2. Color Palette & Roles

```yaml
colors:
  # Surfaces (Linear-adjacent near-black stack)
  canvas: "#010102"           # page background
  surface-1: "#0c0d10"        # header, secondary chrome
  surface-2: "#12141a"        # panes / cards
  surface-3: "#181b24"        # elevated controls, mode active
  surface-4: "#1e2230"        # input fill, nested steps

  # Lines
  hairline: "rgba(255,255,255,0.06)"
  hairline-strong: "rgba(255,255,255,0.12)"
  hairline-focus: "#5e6ad2"

  # Ink
  ink: "#f4f5f7"
  ink-secondary: "#c5cad3"
  ink-muted: "#8b919c"
  ink-faint: "#5c6370"

  # Brand / interactive (Linear lavender — single chromatic accent)
  accent: "#5e6ad2"
  accent-hover: "#828fff"
  accent-soft: "rgba(94,106,210,0.15)"
  on-accent: "#ffffff"

  # Semantic decisions (Raycast category accents, Resend severity)
  deny: "#ff6161"
  deny-soft: "rgba(255,97,97,0.14)"
  allow: "#59d499"
  allow-soft: "rgba(89,212,153,0.14)"
  prompt: "#ffc533"
  prompt-soft: "rgba(255,197,51,0.14)"
  warn: "#ffc533"

  # Focus
  ring: "0 0 0 2px rgba(94,106,210,0.55)"
```

### Decision mapping

| Cap'n Decision | Badge surface | Text / border |
|----------------|---------------|--------------|
| deny (0) | deny-soft | deny |
| allow (1) | allow-soft | allow |
| prompt (2) | prompt-soft | prompt |

### Do not

- Use brand-green (Mintlify/VoltAgent emerald) as the primary CTA — that fights
  ALLOW green. Lavender = interactive; green = allow only.
- Rainbow gradient heroes on private Pages (keep tool chrome).

## 3. Typography Rules

```yaml
typography:
  ui:
    fontFamily: "Inter, ui-sans-serif, system-ui, -apple-system, Segoe UI, sans-serif"
    # Raycast/Linear product sans; Inter if available, else system
  mono:
    fontFamily: "ui-monospace, SFMono-Regular, 'Cascadia Code', 'IBM Plex Mono', Menlo, monospace"
  scale:
    display: { size: 1.5rem, weight: 600, tracking: -0.02em }   # landing h1 only
    title:   { size: 1.125rem, weight: 600, tracking: -0.01em } # page titles
    pane:    { size: 0.7rem, weight: 600, tracking: 0.06em, transform: uppercase } # pane labels
    body:    { size: 0.875rem, weight: 400, lineHeight: 1.5 }
    label:   { size: 0.75rem, weight: 500, color: ink-muted }
    code:    { size: 0.8125rem, weight: 400, lineHeight: 1.4 }
    micro:   { size: 0.6875rem, weight: 500, tracking: 0.04em }
  features:
    ui: '"cv11", "ss01", "kern", "liga"'
```

## 4. Component Stylings

### Buttons

| Variant | Use | Style |
|---------|-----|-------|
| primary | Load evaluator, Evaluate | fill accent, on-accent text, 6px radius, no shadow |
| ghost | Share, secondary | hairline border, transparent fill, ink |
| danger-soft | none as CTA | only inside DENY badge |

Hover: surface-3 for ghost; accent-hover for primary. Disabled: 0.45 opacity.

### Mode toggle (Probe | Author)

Raycast/Linear segmented control: shared hairline border, 8px outer radius,
active segment surface-3 + ink, inactive ink-muted. Keyboard-focusable.

### Panes / cards

- Background surface-2, border 1px hairline, radius 8px (tighter than marketing).
- Pane header: uppercase micro label, hairline bottom divider.
- No drop shadows (Linear restraint); elevation = surface step only.

### Inputs / argv editor

- Mono font, surface-4 fill, hairline border, 6px radius.
- Focus: border accent + ring.
- Argv: one token per line; hit tokens use deny-soft background + left 2px deny bar.

### Decision badge

- Large weight 700, letter-spacing 0.08em, min-width 6.5rem.
- Soft semantic fill + solid semantic border (see mapping).

### Trace steps (Sentry density)

- Nested surface-4 cards, mono head line: `seq · layer · phase · name`.
- Phase tint: enter=ink-muted, match=deny, decide=accent, skip=ink-faint.
- shortCircuit tag: prompt micro uppercase.

### Suite strip

- pass = allow text + check mark
- fail = deny text
- active fixture = accent text
- max-height scroll; sticky header with Run suite

### Encyclopedia (Mintlify density)

- Index table / cards with ordinal mono + name sans.
- Code pages: title = name, eyebrow = ordinal, body notes, link to pack source path mono.

## 5. Layout Principles

```yaml
spacing:
  xs: 4px
  sm: 8px
  md: 12px
  lg: 16px
  xl: 24px
  2xl: 32px

grid:
  play: "minmax(260px,1fr) minmax(260px,1fr) minmax(300px,1.15fr)"
  gap: 12px
  header: 48px height
  max-content-landing: 720px
  max-content-play: none (full width tool)
```

**Whitespace:** landing breathes; `/play` is intentionally tight (forensic tool).

**Breakpoints:** stack three-pane under 960px (single column, order: request → decision → trace).

## 6. Depth & Elevation

| Level | Token | Use |
|-------|-------|-----|
| 0 | canvas | body |
| 1 | surface-1 | site header |
| 2 | surface-2 | panes |
| 3 | surface-3 | active mode, buttons |
| 4 | surface-4 | inputs, trace nested |

No large box-shadows. Optional 1px inset highlight on top edge of panes:
`inset 0 1px 0 rgba(255,255,255,0.04)`.

## 7. Do's and Don'ts

**Do**

- Keep DENY/ALLOW/PROMPT the loudest color events on the page.
- Mono for anything Cap'n/argv/path/PolicyReason.
- Lazy-load chrome: empty state before WASM is calm (muted copy + one primary CTA).
- Prefer hairlines over fills for subdivision.

**Don't**

- Rainbow gradients, glassmorphism, or marketing hero photography.
- Serif display type in the tool chrome (Resend Domaine is marketing-only — not here).
- Green primary buttons (collides with ALLOW).
- Light mode as default (optional later; v1 dark-only continuous).

## 8. Responsive Behavior

| Width | Behavior |
|-------|----------|
| >= 960px | three-pane grid |
| < 960px | single column; toolbar wraps |
| touch | controls min 40px height |

## 9. Agent Prompt Guide

When editing playground UI:

1. Read this file first; map colors to CSS variables in `src/styles/global.css`.
2. New components use existing tokens only — do not invent hex.
3. Decision UI must use semantic tokens (`--deny` / `--allow` / `--prompt`).
4. Interactive chrome uses `--accent` only.
5. Preserve three-pane information architecture from the product spec.

### Quick token → CSS

| Token | CSS var |
|-------|---------|
| canvas | `--bg` |
| surface-2 | `--bg-pane` |
| surface-3 | `--bg-elev` |
| hairline | `--border` |
| ink | `--text` |
| ink-muted | `--muted` |
| accent | `--accent` |
| deny/allow/prompt | `--deny` / `--allow` / `--prompt` |

### Ready prompts

- "Restyle the playground with DESIGN.md; keep Probe/Author and three panes."
- "Make DENY badges match Raycast red soft fill; keep lavender for Load evaluator only."
- "Encyclopedia index: Mintlify density, mono ordinals, no green CTAs."
