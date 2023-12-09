// tailwind.config.js
/** @type {import('tailwindcss').Config} */
module.exports = {
  corePlugins: {
    preflight: false, // disable Tailwind's reset
  },
  content: ["./docs/site/src/**/*.{js,jsx,ts,tsx}"],
  // darkMode: ["class", '[data-theme="dark"]'], // hooks into docusaurus' dark mode settigns
  theme: {
    extend: {},
  },
  plugins: [],
};
