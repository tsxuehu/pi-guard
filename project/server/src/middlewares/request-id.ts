import { Context, Next } from "koa";

function createRequestId(): string {
  return `${Date.now().toString(36)}-${Math.random().toString(36).slice(2, 10)}`;
}

export async function requestIdMiddleware(ctx: Context, next: Next): Promise<void> {
  ctx.state.requestId = createRequestId();
  await next();
}
