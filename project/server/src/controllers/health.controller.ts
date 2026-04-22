import { Context } from "koa";
import { getHealthStatus } from "../services/health.service";

export function getHealth(ctx: Context): void {
  ctx.body = getHealthStatus();
}
