/** @type {import('tailwindcss').Config} */
export default {
  content: [
    "./index.html",
    "./src/**/*.{js,ts,jsx,tsx}",
  ],
  theme: {
    extend: {
      colors: {
        'bg': '#141f27',
        'panel': '#1f2e3c',
        'panel-alt': '#243544',
        'accent-gold': '#c5a23d',
        'accent-gold-soft': '#d4b25a',
        'blue': '#3c5a72',
        'blue-light': '#567a95',
        'grey-light': '#c7cfd6',
        'grey-mid': '#7e8891',
        'grey-dark': '#2e3740',
        'success': '#4aae65',
        'error': '#c24040',
        'warning': '#d18634',
        'text': '#e5e9ec',
        'text-dim': '#b3bbc2',
      },
      boxShadow: {
        'soft': '0 4px 12px rgba(0, 0, 0, 0.25)',
      },
      borderRadius: {
        'sm': '4px',
        'md': '8px',
        'lg': '16px',
      },
      transitionDuration: {
        'DEFAULT': '180ms',
      },
    },
  },
  plugins: [],
}
