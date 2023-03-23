import { DataFirebaseApi } from "./data.firebase.api";
import { ParseMeta } from "papaparse";

export type DataMetadata = ParseMeta & { numRows: number; id?: string };

export interface Data {
  metadata: DataMetadata;
  title: string;
  pathRef: string;
  size: number;
  createdBy: string;
  createdAt: Date;
}

export interface DataApi {
  update(dataId: string, data: any): Promise<void>;
  list(): Promise<Data[]>;
  delete(dataId: string): Promise<void>;
  uploadFile(file: File): Promise<void>;
}

export const dataApi: DataApi = new DataFirebaseApi();
