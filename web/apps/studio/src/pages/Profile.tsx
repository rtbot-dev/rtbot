import { authenticated } from "./authenticated";

export const Profile = authenticated(() => <div className="flex items-center justify-center h-screen">Profile</div>);
