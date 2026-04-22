import Router from "@koa/router";
import healthRouter from "./health";

const router = new Router();

router.use(healthRouter.routes(), healthRouter.allowedMethods());

export default router;
