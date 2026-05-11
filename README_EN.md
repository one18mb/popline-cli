# PopLine CLI

`pln` — command-line tool for JSON ↔ PopLine conversion and validation.

## Install

```bash
go install github.com/one18mb/popline-cli@latest
```

Or download pre-built binaries from [Releases](https://github.com/one18mb/popline-cli/releases).

## Usage

```bash
# JSON → PopLine
pln convert package.json package.pln

# PopLine → JSON
pln convert config.pln config.json

# Validate a PopLine file
pln validate schema.pln

# Help
pln help
```

## Example

```bash
echo '{"name":"popline","version":2}' | tee /tmp/in.json
pln convert /tmp/in.json /tmp/out.pln
cat /tmp/out.pln
```

Output:
```
{
name: "popline"
version: 2
```

Size comparison:
```
17011 B  package.json
13080 B  package.pln   (76.9%)
```
