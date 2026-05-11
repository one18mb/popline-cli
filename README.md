# PopLine CLI

`pln` 命令行工具 — JSON ↔ PopLine 互转、校验。

## 安装

```bash
go install github.com/one18mb/popline-cli@latest
```

或下载预编译二进制（见 [Releases](https://github.com/one18mb/popline-cli/releases)）。

## 使用

```bash
# JSON → PopLine
pln convert package.json package.pln

# PopLine → JSON
pln convert config.pln config.json

# 校验 PopLine 文件
pln validate schema.pln

# 帮助
pln help
```

## 示例

```bash
echo '{"name":"popline","version":2}' | tee /tmp/in.json
pln convert /tmp/in.json /tmp/out.pln
cat /tmp/out.pln
```

输出：
```
{
name: "popline"
version: 2
```

体积对比：
```
17011 B  package.json
13080 B  package.pln   (76.9%)
```
