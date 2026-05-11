# PopLine CLI

`pln` 命令行工具 — JSON ↔ PopLine 互转、校验。C 实现，零依赖，单二进制。

## 安装

```bash
# 从源码编译
gcc -O2 -o pln main.c popline.c popline_parser.c popline_json.c cjson/cJSON.c -lm
sudo cp pln /usr/local/bin/

# 或下载预编译二进制（见 Releases）
```

依赖：`gcc`、`make`（可选），**无需**任何第三方库（cJSON 已内置）。

## 使用

```bash
# JSON → PopLine（体积减 23%）
pln convert package.json package.pln

# PopLine → JSON
pln convert config.pln config.json

# 校验
pln validate schema.pln

# 帮助
pln help
```

## 示例

```bash
echo '{"name":"popline","version":2}' > /tmp/in.json
pln convert /tmp/in.json /tmp/out.pln
cat /tmp/out.pln
```

输出：
```
{
name: "popline"
version: 2
```

## 性能

17011 B JSON → 13074 B PopLine（**76.9%**），转换耗时微秒级。

## 文件

| 文件 | 说明 |
|------|------|
| `main.c` | CLI 入口 |
| `popline.c` | PopLine 核心 |
| `popline_parser.c` | PopLine 解析器 |
| `popline_json.c` | JSON 转换 |
| `cjson/cJSON.c` | cJSON 库（内置） |
