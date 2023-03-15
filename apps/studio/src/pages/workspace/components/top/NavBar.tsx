import menu from "@/store/menu";
import { FaUser, MdMenu } from "react-icons/all";
import { signOut } from "../../../../api/firebase";

export const NavBar = () => {
  return (
    <div className="navbar bg-base-100">
      <div className="flex-none">
        <button className="btn btn-square btn-ghost" onClick={() => menu.toggle()}>
          <MdMenu className="w-full" />
        </button>
      </div>
      <div className="flex-1">
        <img src="/rtbot-logo.svg" className="h-6 mr-3 sm:h-9" alt="RtBot Logo" />
      </div>
      <div className="dropdown dropdown-end">
        <label tabIndex={0} className="btn btn-ghost btn-circle">
          <div className="w-10 rounded-full flex justify-center content-center">
            <FaUser />
          </div>
        </label>
        <ul tabIndex={0} className="mt-3 p-2 shadow menu menu-compact dropdown-content bg-base-100 rounded-box w-52">
          <li>
            <a className="justify-between">
              Profile
              <span className="badge">New</span>
            </a>
          </li>
          <li>
            <a>Settings</a>
          </li>
          <li>
            <a onClick={signOut}>Logout</a>
          </li>
        </ul>
      </div>
    </div>
  );
};
