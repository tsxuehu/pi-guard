export {};

declare module "koa" {
  interface DefaultState {
    requestId?: string;
  }
}
