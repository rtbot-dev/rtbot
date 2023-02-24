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
  operatorType: z.literal("MovingAverage"),
  parameters: z.object({
    M: parameterSchema.merge(z.object({ value: z.number().min(0) })).omit({ description: true }),
    N: parameterSchema.merge(z.object({ value: z.number().min(0) })).omit({ description: true }),
  }),
});
export const extendedMovingAverageSchema = baseOperatorSchema.merge(movingAverageSchema);

export const standardDeviationSchema = baseOperatorSchema.merge(
  z.object({
    title: z.literal("Standard Deviation"), // this will appear in the form
    operatorType: z.literal("StandardDeviation"),
    parameters: z.object({
      M: parameterSchema.merge(z.object({ value: z.number().min(0) })).omit({ description: true }),
      N: parameterSchema.merge(z.object({ value: z.number().min(0) })).omit({ description: true }),
    }),
  })
);

export const inputSchema = baseOperatorSchema.merge(
  z.object({
    title: z.literal("Input"),
    operatorType: z.literal("Input"),
    parameters: z.object({
      resampler: parameterSchema.merge(z.object({ value: z.enum(["None", "Hermite", "Chebyshev"]) })),
      sameTimestampInEntryPolicy: parameterSchema.merge(z.object({ value: z.enum(["Ignore", "Forward"]) })),
    }),
  })
);

const operatorSchemas = [movingAverageSchema, standardDeviationSchema, inputSchema];
export const formOperatorSchema = z.union(operatorSchemas.map((op) => op.pick({ title: true, parameters: true })));
export const operatorSchema = z.union(operatorSchemas);
export type Operator = z.infer<typeof operatorSchema>;
