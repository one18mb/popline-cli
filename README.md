# PopLine CLI

`pln` 命令行工具 — PopLine 与 JSON/YAML/TOML/INI/XML 互转、校验。C 实现，零依赖，单二进制。

**v0.4.0 架构更新：** `to` 方向改用 SAX 接口（`pln_sax_parse`），单遍零 DOM 转换；`from` 方向使用 PopLine DOM（`pln_value_t`）。

## 安装

```bash
# 从源码编译
gcc -O2 -o pln main.c popline.c popline_parser.c popline_sax.c sax_formats.c fmt_json.c fmt_ini.c fmt_xml.c fmt_yaml.c fmt_toml.c cjson/cJSON.c -lm -lexpat
sudo cp pln /usr/local/bin/

# 或下载预编译二进制（见 Releases）
```

依赖：`gcc`，XML 需要 `libexpat-dev`。**无需**其他第三方库（cJSON 已内置）。

## 使用

```bash
# PopLine → JSON（零 DOM，SAX 单遍转换）
pln to json config.pln config.json

# JSON → PopLine（pln_value_t DOM → pln_dumps）
pln from json package.json package.pln

# 校验（SAX 解析，不构建 DOM）
pln validate schema.pln

# 支持格式：json, yaml, toml, ini, xml
pln to yaml config.pln config.yaml
pln from toml config.toml config.pln
```

## 示例

```bash
echo '{"name":"popline","version":2}' > /tmp/in.json
pln from json /tmp/in.json /tmp/out.pln
cat /tmp/out.pln
```

输出：
```
{
name: "popline"
version: 2
```

## 文件

| 文件 | 说明 |
|------|------|
| `main.c` | CLI 入口（`to` 调 SAX，`from` 调 DOM） |
| `popline_sax.c` | SAX 解析器 |
| `sax_formats.c` | SAX 格式转换（`sax_to_json/yaml/...`） |
| `fmt_json.c` | JSON → PopLine DOM 解析 |
| `fmt_yaml.c` | YAML → PopLine DOM 解析 |
| `fmt_toml.c` | TOML → PopLine DOM 解析 |
| `fmt_ini.c` | INI → PopLine DOM 解析 |
| `fmt_xml.c` | XML → PopLine DOM 解析（依赖 expat） |
| `popline.c` | PopLine 核心 |
| `popline_parser.c` | DOM 解析器 |
| `cjson/cJSON.c` | cJSON 库（内置，仅 JSON 解析使用） |

## 致谢
本项目的开发得到了以下 AI 工具的大力协助：
- [Claude Code](https://claude.ai)（Anthropic）
- [DeepSeek](https://deepseek.com)（深度求索）
