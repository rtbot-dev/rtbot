import { z } from "zod";
import zodToJsonSchema from "zod-to-json-schema";
import { operatorSchema } from "./operator.schemas";

export const connectionSchema = z.object({
  from: z.string(),
  to: z.string(),
});
export type Connection = z.infer<typeof connectionSchema>;

export const programMetadataSchema = z.object({
  id: z.string().optional(),
  title: z.string().optional(),
  description: z.string().optional(),
  createdAt: z.date().optional(),
  updatedAt: z.date().optional(),
  createdBy: z.string().optional(),
  updatedBy: z.string().optional(),
  version: z.string().optional(),
  author: z.string().optional(),
  license: z.string().optional(),
});
export const programSchema = z.object({
  metadata: programMetadataSchema.optional(),
  entryNode: z.string().optional(),
  operators: z.array(operatorSchema),
  connections: z.array(connectionSchema),
});
export type Program = z.infer<typeof programSchema>;
