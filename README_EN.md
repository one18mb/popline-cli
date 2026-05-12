# PopLine CLI

`pln` — JSON ↔ PopLine converter and validator. Written in C, zero dependencies, single binary.

## Install

```bash
# Build from source
gcc -O2 -o pln main.c popline.c popline_parser.c popline_json.c cjson/cJSON.c -lm
sudo cp pln /usr/local/bin/

# Or download pre-built binaries from Releases
```

Requires: `gcc` only. No external libraries (cJSON bundled).

### Pre-built binaries

| File | Platform |
|------|----------|
| `pln-linux-amd64` | Linux x86_64 |
| `pln-windows-amd64.exe` | Windows x86_64 |

## Usage

```bash
# JSON → PopLine (23% smaller)
pln convert package.json package.pln

# PopLine → JSON
pln convert config.pln config.json

# Validate
pln validate schema.pln

# Help
pln help
```

## Example

```bash
echo '{"name":"popline","version":2}' > /tmp/in.json
pln convert /tmp/in.json /tmp/out.pln
cat /tmp/out.pln
```

Output:
```
{
name: "popline"
version: 2
```

## Performance

17011 B JSON → 13074 B PopLine (**76.9%**), conversion takes microseconds.

## Files

| File | Description |
|------|-------------|
| `main.c` | CLI entry point |
| `popline.c` | PopLine core |
| `popline_parser.c` | PopLine parser |
| `popline_json.c` | JSON conversion |
| `cjson/cJSON.c` | Bundled cJSON library |

## Acknowledgments
This project was developed with the assistance of:
- [Claude Code](https://claude.ai) (Anthropic)
- [DeepSeek](https://deepseek.com) (DeepSeek)
