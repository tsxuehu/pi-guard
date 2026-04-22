type LogLevel = "info" | "warn" | "error";

function format(level: LogLevel, message: string): string {
  return `[${new Date().toISOString()}] [${level.toUpperCase()}] ${message}`;
}

export const logger = {
  info(message: string): void {
    console.log(format("info", message));
  },
  warn(message: string): void {
    console.warn(format("warn", message));
  },
  error(message: string): void {
    console.error(format("error", message));
  },
};
