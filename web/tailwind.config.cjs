/** @type {import('tailwindcss').Config} */
module.exports = {
  plugins: [require("daisyui")],
  content: [
    "./apps/studio/index.html",
    "./apps/studio/src/**/*.{js,ts,jsx,tsx}",
    "./libs/windows-manager/index.html",
    "./libs/windows-manager/src/**/*.{js,ts,jsx,tsx}",
  ],
}
