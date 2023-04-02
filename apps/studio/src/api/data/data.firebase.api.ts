import { Data, DataApi, DataMetadata } from "./index";
import {
  getStorage,
  ref,
  uploadBytesResumable,
  getDownloadURL,
  FirebaseStorage,
  deleteObject,
  getBlob,
  getStream,
} from "firebase/storage";
import { User } from "firebase/auth";
import menu from "@/store/menu";
import auth from "@/store/auth";
import { app } from "@/api/firebase";
import {
  addDoc,
  collection,
  deleteDoc,
  doc,
  getDoc,
  getDocs,
  getFirestore,
  orderBy,
  query,
  serverTimestamp,
  updateDoc,
  where,
} from "firebase/firestore";
import { format } from "date-fns";
import Papa, { ParseMeta, ParseResult } from "papaparse";

export class DataFirebaseApi implements DataApi {
  private user: User | null = null;
  private readonly storage: FirebaseStorage;
  private readonly firestore;
  private readonly dataCache: { [key: string]: number[][] } = {};
  constructor() {
    this.storage = getStorage(app);
    this.firestore = getFirestore(app);
    auth.subscribe(async (authState) => {
      this.user = authState.user;
    });
  }
  async uploadFile(file: File): Promise<void> {
    if (this.user === null) return;

    let parseMeta: DataMetadata;
    try {
      const data: ParseResult<any> = await new Promise((resolve, reject) => {
        Papa.parse(file, {
          worker: true,
          complete(result) {
            resolve(result);
          },
          error(error: Error) {
            reject(error);
          },
        });
      });
      parseMeta = { ...data.meta, numRows: data.data.length };
      console.log("csv metadata", parseMeta);
    } catch (e) {
      console.log("File is not valid csv document");
    }

    const pathRef = `${this.user.uid}/${format(new Date(), "yyyy/MM/dd")}/${Date.now()}_${file.name}`;
    const storageRef = ref(this.storage, pathRef);
    const uploadTask = uploadBytesResumable(storageRef, file);

    // Listen for state changes, errors, and completion of the upload.
    uploadTask.on(
      "state_changed",
      (snapshot) => {
        // Get task progress, including the number of bytes uploaded and the total number of bytes to be uploaded
        const progress = snapshot.bytesTransferred / snapshot.totalBytes;
        menu.setUploadProgress(progress);
        console.log("Upload is " + progress + "% done");
        switch (snapshot.state) {
          case "paused":
            console.log("Upload is paused");
            break;
          case "running":
            console.log("Upload is running");
            break;
        }
      },
      (error) => {
        // A full list of error codes is available at
        // https://firebase.google.com/docs/storage/web/handle-errors
        switch (error.code) {
          case "storage/unauthorized":
            // User doesn't have permission to access the object
            break;
          case "storage/canceled":
            // User canceled the upload
            break;

          // ...

          case "storage/unknown":
            // Unknown error occurred, inspect error.serverResponse
            break;
        }
      },
      async () => {
        menu.setUploadProgress(0);
        try {
          const docRef = await addDoc(collection(this.firestore, `data`), {
            title: file.name.replace(".csv", ""),
            metadata: parseMeta,
            pathRef,
            size: file.size,
            createdBy: this.user!!.uid,
            createdAt: serverTimestamp(),
          });
          console.log("Program saved with id: ", docRef.id);
        } catch (e) {
          console.error("Error adding program: ", e);
        }
      }
    );
  }

  async list(): Promise<Data[]> {
    if (this.user === null) {
      console.error("list data: user is not authenticated");
      return Promise.resolve([]);
    }

    try {
      const querySnapshot = await getDocs(
        query(collection(this.firestore, `data`), where("createdBy", "==", this.user.uid), orderBy("createdAt"))
      );
      const data: Data[] = [];
      querySnapshot.forEach((doc) => {
        const dataDoc = doc.data();
        const metadata = { ...dataDoc.metadata, id: doc.id };
        data.push({ ...doc.data(), metadata } as Data);
      });
      return data;
    } catch (e) {
      console.error("Error while getting the list of data", e);
    }
    return [];
  }

  async update(dataId: string, data: any): Promise<void> {
    if (this.user === null) {
      console.error("updating data: user is not authenticated");
      return;
    }

    try {
      console.log("updating", dataId, data);
      await updateDoc(doc(this.firestore, `data/${dataId}`), {
        ...data,
        updatedBy: this.user.uid,
        updatedAt: serverTimestamp(),
      });
    } catch (e) {
      console.error("Error while updating data", dataId, e);
    }
  }

  async delete(dataId: string): Promise<void> {
    if (this.user === null) {
      console.error("Delete data: user is not authenticated");
      return;
    }
    try {
      // get the pathRef associated to the data id
      const dbRef = await getDoc(doc(this.firestore, `data/${dataId}`));
      const { pathRef } = dbRef.data() as any;
      // delete the actual file
      await deleteObject(ref(this.storage, pathRef));
      // delete reference in database
      await deleteDoc(doc(this.firestore, `data/${dataId}`));
      console.log("Data deleted", dataId);
    } catch (e) {
      console.error("Error deleting data: ", e);
    }
  }

  async load(dataId: string): Promise<number[][]> {
    if (this.dataCache[dataId]) {
      console.log("Using cached version of data", dataId);
      return this.dataCache[dataId];
    }

    if (this.user === null) {
      console.error("Load data: user is not authenticated");
      return [];
    }

    try {
      // get the pathRef associated to the data id
      const dbRef = await getDoc(doc(this.firestore, `data/${dataId}`));
      const { pathRef } = dbRef.data() as any;
      // delete the actual file
      const csvBlob = await getBlob(ref(this.storage, pathRef));

      const data: number[][] = await new Promise(async (resolve, reject) => {
        Papa.parse(await csvBlob.text(), {
          worker: true,
          complete(result: ParseResult<string[]>) {
            // parse the result with the expected format for the columns:
            // first colum is an integer representing the timestamp
            // and remaining ones are considered numeric columns
            const numeric = result.data
              .filter((r: string[]) => !isNaN(r[0] as any) && r.length > 1)
              .map((r) => [parseInt(r[0]), ...r.slice(1).map((v) => parseFloat(v))]);
            resolve(numeric);
          },
          error(error: Error) {
            reject(error);
          },
        });
      });
      this.dataCache[dataId] = data;
      console.log("Data downloaded and cached", dataId, data);
      return data;
    } catch (e) {
      console.error("Error deleting data: ", e);
    }

    return Promise.resolve([]);
  }

  async clearCache() {
    console.log("Clearing local data cache");
    Object.keys(this.dataCache).forEach((k) => delete this.dataCache[k]);
  }
}
