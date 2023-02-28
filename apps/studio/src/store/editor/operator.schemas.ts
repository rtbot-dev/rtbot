import { z } from "zod";

export const parameterTypeSchema = z.enum(["INTEGER", "FLOAT"]);
export type ParameterType = z.infer<typeof parameterTypeSchema>;
export const parameterSchema = z.object({
  description: z.string().optional(),
  value: z.number(),
});
export type Parameter = z.infer<typeof parameterSchema>;

const baseOperatorSchema = z.object({ id: z.string() });
export const movingAverageSchema = z.object({
  title: z.literal("Moving Average"), // this will appear in the form
  Mwindow: z.number().min(0),
  N: z.number().min(0),
});

export const standardDeviationSchema = z.object({
  title: z.literal("Standard Deviation"), // this will appear in the form
  M: z.number().min(0),
  N: z.number().min(0),
});

export const inputSchema = z.object({
  title: z.literal("Input"),
  resampler: z.enum(["None", "Hermite", "Chebyshev"]),
  sameTimestampInEntryPolicy: z.enum(["Ignore", "Forward"]),
});

export const operatorSchemas = [movingAverageSchema, standardDeviationSchema, inputSchema];
export const formOperatorSchema = z.union(operatorSchemas);
export const operatorSchema = z.union(operatorSchemas);
export type Operator = z.infer<typeof operatorSchema>;
