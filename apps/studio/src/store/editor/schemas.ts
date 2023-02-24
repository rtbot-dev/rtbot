import { z } from "zod";
import zodToJsonSchema from "zod-to-json-schema";
import { operatorSchema } from "./operator.schemas";

export const connectionSchema = z.object({
  from: z.string(),
  to: z.string(),
});
export type Connection = z.infer<typeof connectionSchema>;

export const programSchema = z.object({
  title: z.string().optional(),
  description: z.string().optional(),
  date: z.date().optional(),
  version: z.string().optional(),
  author: z.string().optional(),
  license: z.string().optional(),
  entryNode: z.string().optional(),
  operators: z.array(operatorSchema),
  connections: z.array(connectionSchema),
});
export type Program = z.infer<typeof programSchema>;
