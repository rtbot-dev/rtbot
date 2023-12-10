import React from "react";
import Link from "@docusaurus/Link";
import useDocusaurusContext from "@docusaurus/useDocusaurusContext";
import Layout from "@theme/Layout";

import { Player } from "../components/player";

function HomepageHeader() {
  const { siteConfig } = useDocusaurusContext();
  return (
    <header className="py-5">
      <div className="container mx-auto text-center">
        <h1 className="pt-5 text-6xl font-mono font-bold text-white">
          {siteConfig.title}
        </h1>
        <p className="text-2xl py-6 font-mono text-white">
          {siteConfig.tagline}
        </p>

        <div className="pt-2 pb-5">
          <Link
            className="bg-sky-700 rounded-md text-white px-4 py-2"
            style={{ textDecoration: "none" }}
            to="/docs/intro"
          >
            Get Started
          </Link>
        </div>
      </div>
    </header>
  );
}

export default function Home() {
  const { siteConfig } = useDocusaurusContext();
  return (
    <Layout title={`${siteConfig.title}`} description="RtBot">
      <HomepageHeader />
      <main>
        <Player />
      </main>
    </Layout>
  );
}
