import { Menu } from "@headlessui/react";
import React from "react";
const links = [
  { href: "/account-settings", label: "Account settings" },
  { href: "/support", label: "Support" },
  { href: "/license", label: "License" },
  { href: "/sign-out", label: "Sign out" },
];

export const UserMenu = () => {
  return (
    <Menu>
      <Menu.Button>Options</Menu.Button>
      <Menu.Items>
        {links.map((link) => (
          /* Use the `active` state to conditionally style the active item. */
          <Menu.Item key={link.href} as={React.Fragment}>
            {({ active }) => (
              <a href={link.href} className={`${active ? "bg-blue-500 text-white" : "bg-white text-black"}`}>
                {link.label}
              </a>
            )}
          </Menu.Item>
        ))}
      </Menu.Items>
    </Menu>
  );
};
