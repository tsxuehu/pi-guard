import Router from "@koa/router";
import { getHealth } from "../controllers/health.controller";

const healthRouter = new Router();

healthRouter.get("/health", getHealth);

export default healthRouter;
