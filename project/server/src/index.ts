import http from "http";
import app from "./app";
import { loadEnv } from "./config/env";
import { logger } from "./config/logger";
import { initWsGateway } from "./ws/gateway";

const env = loadEnv();
const server = http.createServer(app.callback());

initWsGateway(server);

server.listen(env.port, () => {
  logger.info(`pi-guard-server listening on port ${env.port} (${env.nodeEnv})`);
});
