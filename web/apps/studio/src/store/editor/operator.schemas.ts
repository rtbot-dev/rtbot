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
  opType: z.literal("MovingAverage"),
  parameters: z.object({
    n: z.number().min(0),
    m: z.number().min(0),
  }),
});

export const standardDeviationSchema = z.object({
  title: z.literal("Standard Deviation"), // this will appear in the form
  opType: z.literal("StandardDeviation"),
  parameters: z.object({
    M: z.number().min(0),
    N: z.number().min(0),
  }),
});

export const inputSchema = z.object({
  title: z.literal("Input"),
  opType: z.literal("Input"),
});

export const cosineResamplerSchema = z.object({
  title: z.literal("Resampler Cosine"),
  opType: z.literal("CosineResampler"),
  parameters: z.object({
    dt: z.number().gt(1).int("dt must be an integer"),
  }),
});

export const hermiteResamplerSchema = z.object({
  title: z.literal("Resampler Hermite"),
  opType: z.literal("HermiteResampler"),
  parameters: z.object({
    dt: z.number().gt(1).int("dt must be an integer"),
  }),
});

export const joinSchema = z.object({
  title: z.literal("Join"),
  opType: z.literal("Join"),
  parameters: z.object({
    nInput: z.number().gt(1),
  }),
});

export const localExtremeSchema = z.object({
  title: z.literal("Local Extreme"),
  opType: z.literal("PeakDetector"),
  parameters: z.object({
    n: z.number().gt(0),
  }),
});

export const differenceSchema = z.object({
  title: z.literal("Difference"),
  opType: z.literal("Difference"),
  parameters: z.object({
    nInput: z.number().gt(1).lte(2), // 2
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
      legend: z.string().optional(),
      mode: z.string().optional(),
      stack: z.number().default(0),
    })
    .optional(),
  editing: z.boolean().optional(),
  plot: z.boolean().optional(),
  source: z.string().optional(),
});

export type Metadata = z.infer<typeof metadataSchema>;
export const baseOperatorSchema = z.object({
  id: z.string(),
  title: z.string(),
  opType: z.string(),
  metadata: metadataSchema,
  parameters: z.any(),
});
export type BaseOperator = z.infer<typeof baseOperatorSchema>;

export const operatorSchemaList = [
  movingAverageSchema,
  standardDeviationSchema,
  inputSchema,
  cosineResamplerSchema,
  hermiteResamplerSchema,
  joinSchema,
  localExtremeSchema,
  differenceSchema,
].map((s) => baseOperatorSchema.merge(s));
