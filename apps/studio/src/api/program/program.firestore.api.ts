import { ProgramApi } from "./index";
import { Program } from "@/store/editor/schemas";
import { getFirestore, collection, doc, addDoc, getDoc, getDocs, deleteDoc, updateDoc } from "firebase/firestore";
import { app } from "@/api/firebase";
import auth from "@/store/auth";
import { User } from "firebase/auth";

export class ProgramFirestoreApi implements ProgramApi {
  private readonly firestore;
  private user: User | null = null;

  constructor() {
    this.firestore = getFirestore(app);
    auth.subscribe(async (authState) => {
      this.user = authState.user;
    });
  }
  async create(program: Program): Promise<void> {
    if (this.user === null) {
      console.error("create program: user is not authenticated");
      return;
    }
    try {
      const docRef = await addDoc(collection(this.firestore, `users/${this.user.uid}/programs`), program);
      console.log("Program saved with id: ", docRef.id);
    } catch (e) {
      console.error("Error adding program: ", e);
    }
  }

  async delete(programId: string): Promise<void> {
    if (this.user === null) {
      console.error("create program: user is not authenticated");
      return;
    }
    try {
      const docRef = await deleteDoc(doc(this.firestore, `users/${this.user.uid}/programs/${programId}`));
      console.log("Program deleted", programId);
    } catch (e) {
      console.error("Error deleting program: ", e);
    }
  }

  get(programId: string): Promise<Program | null> {
    return Promise.resolve(null);
  }

  async list(): Promise<Program[]> {
    if (this.user === null) {
      console.error("create program: user is not authenticated");
      return Promise.resolve([]);
    }

    try {
      const querySnapshot = await getDocs(collection(this.firestore, `users/${this.user.uid}/programs`));
      const programs: Program[] = [];
      querySnapshot.forEach((doc) => {
        const data = doc.data();
        const metadata = { ...data.metadata, id: doc.id };
        programs.push({ ...doc.data(), metadata } as Program);
      });
      return programs;
    } catch (e) {
      console.error("Error while getting the list of programs", e);
    }
    return Promise.resolve([]);
  }

  async update(programId: string, program: Partial<Program>): Promise<void> {
    if (this.user === null) {
      console.error("updating program: user is not authenticated");
      return;
    }

    try {
      await updateDoc(doc(this.firestore, `users/${this.user.uid}/programs/${programId}`), program);
    } catch (e) {
      console.error("Error while updating program", programId, e);
    }
  }
}

export const programFirestoreApi = new ProgramFirestoreApi();
