export interface HealthStatus {
  service: string;
  status: "ok";
  timestamp: string;
}

export function getHealthStatus(): HealthStatus {
  return {
    service: "pi-guard-server",
    status: "ok",
    timestamp: new Date().toISOString(),
  };
}
