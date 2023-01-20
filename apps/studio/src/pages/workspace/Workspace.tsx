import { signOut } from "@/api/firebase";
import { authenticated } from "@/pages/authenticated";
import { UserMenu } from "@/pages/workspace/components/UserMenu";

export const Workspace = authenticated(() => (
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
));
