import { signOut } from "@/api/firebase";
import { authenticated } from "@/pages/authenticated";
import { UserMenu } from "@/pages/workspace/components/UserMenu";
import { ProgramEditor } from "./components/ProgramEditor";
import { GraphEditor } from "./components/GraphEditor";

export const Workspace = authenticated(() => (
  <div>
    <nav className="bg-white border-gray-200 px-2 sm:px-4 py-2.5 rounded dark:bg-gray-900">
      <div className="container flex flex-wrap items-center justify-between mx-auto">
        <a href="https://flowbite.com/" className="flex items-center">
          <img src="/rtbot-logo.svg" className="h-6 mr-3 sm:h-9" alt="RtBot Logo" />
        </a>
        <div className="items-center justify-between hidden w-full md:flex md:w-auto md:order-1" id="mobile-menu-2">
          <ul className="flex flex-col p-4 mt-4 border border-gray-100 rounded-lg bg-gray-50 md:flex-row md:space-x-8 md:mt-0 md:text-sm md:font-medium md:border-0 md:bg-white dark:bg-gray-800 md:dark:bg-gray-900 dark:border-gray-700">
            <li>
              <a
                href="#"
                className="block py-2 pl-3 pr-4 text-white bg-blue-700 rounded md:bg-transparent md:text-blue-700 md:p-0 dark:text-white"
                aria-current="page"
              >
                Home
              </a>
            </li>
            <li>
              <button
                type="button"
                onClick={signOut}
                className="text-white bg-gray-800 hover:bg-gray-900 focus:outline-none focus:ring-4 focus:ring-gray-300 font-medium rounded-lg text-sm px-5 py-2.5 mr-2 mb-2 dark:bg-gray-800 dark:hover:bg-gray-700 dark:focus:ring-gray-700 dark:border-gray-700"
              >
                Sign out
              </button>
            </li>
          </ul>
        </div>
        <UserMenu />
      </div>
    </nav>
    <div className="not-prose relative bg-slate-50 rounded-xl overflow-hidden dark:bg-slate-800/25">
      <div
        style={{ backgroundPosition: "10px 10px;" }}
        className="absolute inset-0 bg-grid-slate-100 [mask-image:linear-gradient(0deg,#fff,rgba(255,255,255,0.6))] dark:bg-grid-slate-700/25 dark:[mask-image:linear-gradient(0deg,rgba(255,255,255,0.1),rgba(255,255,255,0.5))]"
      ></div>
      <div className="relative p-8">
        <div className="w-full flex justify-center">
          <div
            className="resize overflow-auto text-sm p-2 ring-1 ring-slate-900/10 shadow-sm dark:bg-slate-800 dark:ring-0 dark:highlight-white/5"
            style={{ height: "90vh", width: "100%", margin: "10px" }}
          >
            <GraphEditor />
          </div>
          <div
            className="resize overflow-auto text-sm p-2 ring-1 ring-slate-900/10 shadow-sm dark:bg-slate-800 dark:ring-0 dark:highlight-white/5"
            style={{ height: "90vh", width: "100vh", margin: "10px" }}
          >
            <ProgramEditor />
          </div>
        </div>
      </div>
      <div className="absolute inset-0 pointer-events-none border border-black/5 rounded-xl dark:border-white/5"></div>
    </div>
  </div>
));
