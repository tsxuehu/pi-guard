export interface EnvConfig {
  nodeEnv: string;
  port: number;
}

export function loadEnv(): EnvConfig {
  return {
    nodeEnv: process.env.NODE_ENV ?? "development",
    port: Number(process.env.PORT ?? 3000),
  };
}
