/** @type {import('tailwindcss').Config} */
export default {
  content: ['./index.html', './src/**/*.{js,jsx,ts,tsx}'],
  theme: {
    extend: {
      fontFamily: {
        sans: ['"DM Sans"', 'Segoe UI', 'system-ui', 'sans-serif'],
        mono: ['"JetBrains Mono"', 'Consolas', 'monospace'],
      },
      colors: {
        // Stratum palette
        accent: {
          DEFAULT: '#f97316',
          light: '#fb923c',
          dark: '#c2410c',
          deeper: '#7c2d12',
        },
        ok: '#22c55e',
        danger: '#ef4444',
        warn: '#f59e0b',
      },
      backgroundImage: {
        // Reusable Stratum surfaces
        'panel': 'linear-gradient(135deg,#2a2a2e 0%,#121214 100%)',
        'panel-toolbar': 'linear-gradient(180deg,#1f1f23 0%,#121214 100%)',
        'panel-recess': 'linear-gradient(180deg,#0a0a0b 0%,#18181b 100%)',
        'panel-raised': 'linear-gradient(180deg,#3f3f46 0%,#27272a 100%)',
        'panel-orange': 'linear-gradient(180deg,#fb923c 0%,#c2410c 100%)',
        'panel-orange-deep': 'linear-gradient(180deg,#9a3412 0%,#7c2d12 100%)',
        'app-bg': 'radial-gradient(circle at 30% -20%,#27272a 0%,#09090b 100%)',
        'workspace-bg': 'radial-gradient(circle at 50% 30%,#1f1f23 0%,#09090b 100%)',
        'brushed': 'repeating-linear-gradient(90deg,#fff,#fff 1px,transparent 1px,transparent 4px)',
        'knob-conic': 'conic-gradient(from 180deg at 50% 50%,#2a2a2e 0deg,#4b4b52 90deg,#18181b 180deg,#4b4b52 270deg,#2a2a2e 360deg)',
      },
      boxShadow: {
        // Stratum elevation
        'panel': 'inset 1px 1px 2px rgba(255,255,255,0.08), inset -1px -1px 3px rgba(0,0,0,0.6), 0 4px 10px -2px rgba(0,0,0,0.7)',
        'chassis': '-16px -16px 40px rgba(63,63,70,0.04), 24px 24px 60px rgba(0,0,0,0.85), inset 1px 1px 2px rgba(255,255,255,0.1), inset -1px -1px 4px rgba(0,0,0,0.7)',
        'recess': 'inset 0 3px 6px rgba(0,0,0,0.9), inset 0 1px 2px rgba(0,0,0,1)',
        'raised': 'inset 0 1px 1px rgba(255,255,255,0.1), 0 4px 8px -2px rgba(0,0,0,0.6)',
        'knob': 'inset 0 2px 3px rgba(255,255,255,0.18), inset 0 -2px 4px rgba(0,0,0,0.7), 0 4px 8px -2px rgba(0,0,0,0.85)',
        'screw': 'inset 0 1px 2px rgba(255,255,255,0.3), 0 2px 4px rgba(0,0,0,0.9)',
        'led-orange': '0 0 12px rgba(249,115,22,0.7), inset 0 1px 1px rgba(255,255,255,0.4), inset 0 -1px 2px rgba(0,0,0,0.4)',
        'led-green': '0 0 8px rgba(34,197,94,0.85), inset 0 1px 1px rgba(255,255,255,0.6)',
        'led-red': '0 0 14px rgba(239,68,68,0.7), inset 0 1px 2px rgba(255,255,255,0.3)',
        'glow-orange': '0 0 10px rgba(249,115,22,0.55)',
      },
      borderRadius: {
        'chassis': '24px',
      },
      animation: {
        'pulse-slow': 'pulse 3s cubic-bezier(0.4, 0, 0.6, 1) infinite',
      },
    },
  },
  plugins: [],
}
