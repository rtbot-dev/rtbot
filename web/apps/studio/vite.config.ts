import { defineConfig } from "vite";
import react from "@vitejs/plugin-react-swc";
import tsconfigPaths from "vite-tsconfig-paths";
import customTsConfig from "vite-plugin-custom-tsconfig";

// https://vitejs.dev/config/
export default defineConfig({
  root: "apps/studio",
  plugins: [
    react(),
    tsconfigPaths(),
    // at run time, this plugin just copies the
    // content of the specified file into
    // a `tsconfig.json` file, which vite will use as default
    customTsConfig({
      tsConfigPath: "tsconfig.bazel.json",
    }),
  ],
});
