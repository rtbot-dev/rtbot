import { z } from "zod";
import { baseOperatorSchema } from "./operator.schemas";

export const connectionSchema = z.object({
  from: z.string(),
  to: z.string(),
});
export type Connection = z.infer<typeof connectionSchema>;

export const programMetadataStyleSchema = z.object({
  type: z.enum(["scattergl"]),
  lineWidth: z.number().gt(0).optional(),
  color: z.string().optional(),
});

export type PlotStyle = z.infer<typeof programMetadataStyleSchema>;

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
  style: programMetadataStyleSchema.optional(),
  computing: z.boolean().optional(),
});
export const programSchema = z.object({
  metadata: programMetadataSchema.optional(),
  entryNode: z.string().optional(),
  operators: z.array(baseOperatorSchema),
  connections: z.array(connectionSchema),
});

export type Program = z.infer<typeof programSchema>;
