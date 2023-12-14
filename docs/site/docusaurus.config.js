// @ts-check
// Note: type annotations allow type checking and IDEs autocompletion

const lightCodeTheme = require("prism-react-renderer/themes/github");
const darkCodeTheme = require("prism-react-renderer/themes/dracula");
const math = require("remark-math");
const katex = require("rehype-katex");

/** @type {import('@docusaurus/types').Config} */
const config = {
  title: "RtBot",
  tagline: "Low Latency, Incremental Analytical Engine",
  favicon: "img/favicon.ico",
  customFields: {
    // Put your custom environment here
    firebaseApiKey: process.env.FIREBASE_API_KEY,
  },

  plugins: [
    async function configureTailwind(context, options) {
      return {
        name: "docusaurus-tailwindcss",
        configurePostCss(postcssOptions) {
          // Appends TailwindCSS and AutoPrefixer.
          postcssOptions.plugins.push(require("tailwindcss"));
          postcssOptions.plugins.push(require("autoprefixer"));
          return postcssOptions;
        },
      };
    },
    async (context, options) => {
      return {
        name: "custom-docusaurus-plugin",
        configureWebpack(config, isServer, utils) {
          return {
            resolve: {
              fallback: {
                path: false,
                fs: false,
              },
            },
          };
        },
      };
    },
    async (context, options) => {
      return {
        configureWebpack(config, isServer, utils, content) {
          const reactPath = process.cwd() + "/node_modules/react";
          return {
            resolve: {
              alias: {
                react: reactPath,
              },
              fallback: {
                crypto: false
              },
            },
          };
        },
      };
    },
  ],
  // Set the production url of your site here
  url: "https://rtbot.dev",
  // Set the /<baseUrl>/ pathname under which your site is served
  // For GitHub pages deployment, it is often '/<projectName>/'
  baseUrl: "/",

  // GitHub pages deployment config.
  // If you aren't using GitHub pages, you don't need these.
  organizationName: "rtbot-dev", // Usually your GitHub org/user name.
  projectName: "rtbot", // Usually your repo name.

  onBrokenLinks: "throw",
  onBrokenMarkdownLinks: "warn",

  presets: [
    [
      "classic",
      /** @type {import('@docusaurus/preset-classic').Options} */
      ({
        docs: {
          remarkPlugins: [math],
          rehypePlugins: [katex],
          sidebarPath: require.resolve("./sidebars.js"),
          // Please change this to your repo.
          // Remove this to remove the "edit this page" links.
          editUrl: "https://github.com/rtbot-dev/rtbot/tree/master/docs/",
        },
        blog: {
          showReadingTime: true,
          // Please change this to your repo.
          // Remove this to remove the "edit this page" links.
          editUrl:
            "https://github.com/facebook/docusaurus/tree/main/packages/create-docusaurus/templates/shared/",
        },
        theme: {
          customCss: [
            require.resolve("./src/css/custom.css"),
            // import css from radix-ui
            require.resolve("@radix-ui/colors/sky.css"),
            require.resolve("@radix-ui/colors/gray.css"),
            require.resolve("@radix-ui/colors/blue.css"),
            require.resolve("@radix-ui/colors/green.css"),
            require.resolve("@radix-ui/colors/yellow.css"),
          ],
        },
      }),
    ],
  ],
  stylesheets: [
    {
      href: "https://cdn.jsdelivr.net/npm/katex@0.13.24/dist/katex.min.css",
      type: "text/css",
      integrity:
        "sha384-odtC+0UGzzFL/6PNoE8rX/SPcQDXBJ+uRepguP4QkPCm2LBxH3FA3y+fKSiJ+AmM",
      crossorigin: "anonymous",
    },
  ],
  themeConfig:
    /** @type {import('@docusaurus/preset-classic').ThemeConfig} */
    ({
      colorMode: {
        defaultMode: "dark",
        disableSwitch: true,
        respectPrefersColorScheme: false,
      },
      // Replace with your project's social card
      image: "img/docusaurus-social-card.jpg",
      navbar: {
        title: "",
        logo: {
          alt: "RtBot Logo",
          src: "img/rtbot-dark-mode.svg",
        },
        items: [
          {
            type: "docSidebar",
            sidebarId: "documentationSidebar",
            position: "left",
            label: "Documentation",
          },
          //{ to: "/blog", label: "Blog", position: "left" },
          {
            href: "https://github.com/rtbot-dev/rtbot",
            label: "GitHub",
            position: "right",
          },
        ],
      },
      footer: {
        style: "dark",
        links: [
          {
            title: "Learn",
            items: [
              {
                label: "Documentation",
                to: "/docs/intro",
              },
            ],
          },
          {
            title: "Community",
            items: [
              {
                label: "Discord",
                href: "https://discord.gg/XSv6mZq7YQ",
              },
              {
                label: "Reddit",
                href: "https://www.reddit.com/r/rtbot/",
              },
            ],
          },
          {
            title: "More",
            items: [
              {
                label: "GitHub",
                href: "https://github.com/rtbot-dev/rtbot",
              },
            ],
          },
        ],
        copyright: `Copyright © ${new Date().getFullYear()} RtBot, Inc.`,
      },
      prism: {
        theme: lightCodeTheme,
        darkTheme: darkCodeTheme,
      },
    }),
};

module.exports = config;
