import { Server as HttpServer } from "http";
import { logger } from "../config/logger";

export function initWsGateway(_server: HttpServer): void {
  // Reserved for WebSocket gateway integration.
  logger.info("ws gateway initialized");
}
