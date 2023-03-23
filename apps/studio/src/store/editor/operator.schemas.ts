import { z, ZodTypeAny } from "zod";

export const parameterTypeSchema = z.enum(["INTEGER", "FLOAT"]);
export type ParameterType = z.infer<typeof parameterTypeSchema>;
export const parameterSchema = z.object({
  description: z.string().optional(),
  value: z.number(),
});
export type Parameter = z.infer<typeof parameterSchema>;

export const movingAverageSchema = z.object({
  title: z.literal("Moving Average"), // this will appear in the form
  opType: z.literal("MOVING_AVERAGE"),
  parameters: z.object({
    MBackWindow: z.number().min(0),
    NForwardWindow: z.number().min(0),
  }),
});

export const standardDeviationSchema = z.object({
  title: z.literal("Standard Deviation"), // this will appear in the form
  opType: z.literal("STANDARD_DEVIATION"),
  parameters: z.object({
    M: z.number().min(0),
    N: z.number().min(0),
  }),
});

export const inputSchema = z.object({
  title: z.literal("Input"),
  opType: z.literal("INPUT"),
  parameters: z.object({
    resampler: z.enum(["None", "Hermite", "Chebyshev"]),
    sameTimestampInEntryPolicy: z.enum(["Ignore", "Forward"]),
  }),
});

export const metadataSchema = z.object({
  position: z
    .object({
      x: z.number().default(0).optional(),
      y: z.number().default(0).optional(),
    })
    .optional(),
  style: z
    .object({
      color: z.string().optional(),
      lineType: z.string().optional(),
      lineWidth: z.number().optional(),
    })
    .optional(),
  editing: z.boolean().optional(),
});

export type Metadata = z.infer<typeof metadataSchema>;
const baseOperatorSchema = z.object({
  id: z.string(),
  title: z.string(),
  opType: z.string(),
  metadata: metadataSchema,
  parameters: z.any(),
});
export type BaseOperator = z.infer<typeof baseOperatorSchema>;

export const operatorSchemaList = [movingAverageSchema, standardDeviationSchema, inputSchema].map((s) =>
  baseOperatorSchema.merge(s)
);
