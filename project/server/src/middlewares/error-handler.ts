import { Context, Next } from "koa";
import { logger } from "../config/logger";

export async function errorHandlerMiddleware(ctx: Context, next: Next): Promise<void> {
  try {
    await next();
  } catch (error) {
    const message = error instanceof Error ? error.message : "unknown error";
    logger.error(`[${ctx.state.requestId ?? "no-request-id"}] ${message}`);
    ctx.status = 500;
    ctx.body = {
      code: "INTERNAL_ERROR",
      message: "Internal server error",
      requestId: ctx.state.requestId ?? null,
    };
  }
}
