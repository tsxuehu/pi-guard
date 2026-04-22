import Koa from "koa";
import router from "./routes";
import { errorHandlerMiddleware } from "./middlewares/error-handler";
import { requestIdMiddleware } from "./middlewares/request-id";

const app = new Koa();

app.use(errorHandlerMiddleware);
app.use(requestIdMiddleware);
app.use(router.routes());
app.use(router.allowedMethods());

export default app;
