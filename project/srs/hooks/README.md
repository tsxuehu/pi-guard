# SRS Hooks

该目录用于存放与 SRS `http_hooks` 配套的脚本或调试材料。

当前 `conf/srs.conf` 中已预留以下回调接口（由 `pi-guard-server` 提供）：

- `POST /api/srs/on_publish`
- `POST /api/srs/on_unpublish`
- `POST /api/srs/on_hls`

如果后续需要本地调试，可在此目录补充 mock 服务或回调日志脚本。
