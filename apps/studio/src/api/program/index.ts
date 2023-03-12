import { Program } from "@/store/editor/schemas";
import { programFirestoreApi } from "./program.firestore.api";

export interface ProgramApi {
  create(program: Program): Promise<void>;
  get(programId: string): Promise<Program | null>;
  list(): Promise<Program[]>;
  update(programId: string, program: Partial<Program>): Promise<void>;
  delete(programId: string): Promise<void>;
}

export const programApi: ProgramApi = programFirestoreApi;
