package main

import (
	"encoding/json"
	"fmt"
	"os"
	"strings"

	"github.com/one18mb/popline-go" // package pln
)

func main() {
	if len(os.Args) < 2 {
		usage()
		os.Exit(1)
	}

	cmd := os.Args[1]
	switch cmd {
	case "convert":
		cmdConvert(os.Args[2:])
	case "validate":
		cmdValidate(os.Args[2:])
	case "help", "--help", "-h":
		usage()
	default:
		fmt.Fprintf(os.Stderr, "unknown command: %s\n", cmd)
		usage()
		os.Exit(1)
	}
}

func usage() {
	fmt.Print(`pln — PopLine command-line tool

Usage:
  pln convert <input> <output>   Convert between JSON and PopLine
  pln validate <file>            Validate a PopLine file
  pln help                       Show this help

Examples:
  pln convert package.json package.pln    JSON → PopLine
  pln convert config.pln config.json      PopLine → JSON
  pln validate schema.pln                 Validate only

File extension determines conversion direction:
  .json → .pln  : JSON to PopLine
  .pln  → .json : PopLine to JSON
`)
}

func cmdConvert(args []string) {
	if len(args) < 2 {
		fmt.Fprintln(os.Stderr, "usage: pln convert <input> <output>")
		os.Exit(1)
	}
	inputPath, outputPath := args[0], args[1]

	data, err := os.ReadFile(inputPath)
	if err != nil {
		fmt.Fprintf(os.Stderr, "error reading %s: %v\n", inputPath, err)
		os.Exit(1)
	}

	inputText := strings.TrimSpace(string(data))
	if inputText == "" {
		fmt.Fprintln(os.Stderr, "error: empty input file")
		os.Exit(1)
	}

	if strings.HasSuffix(inputPath, ".json") && strings.HasSuffix(outputPath, ".pln") {
		convertJSONtoPLN(inputText, outputPath)
	} else if strings.HasSuffix(inputPath, ".pln") && strings.HasSuffix(outputPath, ".json") {
		convertPLNtoJSON(inputText, outputPath)
	} else {
		// Auto-detect: try parsing as JSON first
		var tmp interface{}
		if json.Unmarshal([]byte(inputText), &tmp) == nil {
			convertJSONtoPLN(inputText, outputPath)
		} else {
			convertPLNtoJSON(inputText, outputPath)
		}
	}
}

func convertJSONtoPLN(inputText, outputPath string) {
	var obj interface{}
	if err := json.Unmarshal([]byte(inputText), &obj); err != nil {
		fmt.Fprintf(os.Stderr, "invalid JSON: %v\n", err)
		os.Exit(1)
	}
	v := interfaceToValue(obj)
	output := pln.Marshal(v)
	if err := os.WriteFile(outputPath, []byte(output), 0644); err != nil {
		fmt.Fprintf(os.Stderr, "error writing %s: %v\n", outputPath, err)
		os.Exit(1)
	}
	fmt.Printf("converted %s → %s\n", inputPathOrJSON(outputPath), outputPath)
}

func convertPLNtoJSON(inputText, outputPath string) {
	v, err := pln.Unmarshal(inputText)
	if err != nil {
		fmt.Fprintf(os.Stderr, "invalid PopLine: %v\n", err)
		os.Exit(1)
	}
	obj := valueToInterface(v)
	output, err := json.Marshal(obj)
	if err != nil {
		fmt.Fprintf(os.Stderr, "json marshal error: %v\n", err)
		os.Exit(1)
	}
	output = append(output, byte('\n'))
	if err := os.WriteFile(outputPath, output, 0644); err != nil {
		fmt.Fprintf(os.Stderr, "error writing %s: %v\n", outputPath, err)
		os.Exit(1)
	}
	fmt.Printf("converted → %s\n", outputPath)
}

func cmdValidate(args []string) {
	if len(args) < 1 {
		fmt.Fprintln(os.Stderr, "usage: pln validate <file>")
		os.Exit(1)
	}
	path := args[0]
	data, err := os.ReadFile(path)
	if err != nil {
		fmt.Fprintf(os.Stderr, "error reading %s: %v\n", path, err)
		os.Exit(1)
	}
	_, err = pln.Unmarshal(string(data))
	if err != nil {
		fmt.Fprintf(os.Stderr, "invalid: %v\n", err)
		os.Exit(1)
	}
	fmt.Println("valid")
}

func inputPathOrJSON(path string) string {
	return path
}

// valueToInterface converts pln.Value → Go interface{} for JSON output
func valueToInterface(v *pln.Value) interface{} {
	if v == nil {
		return nil
	}
	switch v.Type {
	case pln.Null:
		return nil
	case pln.Bool:
		return v.Bool()
	case pln.Int:
		return v.Int()
	case pln.Float:
		return v.Float()
	case pln.String:
		return v.Str()
	case pln.Object:
		m := make(map[string]interface{})
		for _, c := range v.Children() {
			m[c.Key()] = valueToInterface(c)
		}
		return m
	case pln.Array:
		a := make([]interface{}, len(v.Children()))
		for i, c := range v.Children() {
			a[i] = valueToInterface(c)
		}
		return a
	}
	return nil
}

// interfaceToValue converts Go interface{} → pln.Value for PopLine output
func interfaceToValue(v interface{}) *pln.Value {
	if v == nil {
		return pln.NewNull()
	}
	switch val := v.(type) {
	case bool:
		return pln.NewBool(val)
	case float64:
		// JSON numbers decode as float64 by default
		if val == float64(int64(val)) {
			return pln.NewInt(int64(val))
		}
		return pln.NewFloat(val)
	case string:
		return pln.NewString(val)
	case []interface{}:
		arr := pln.NewArray()
		for _, item := range val {
			arr.AddToArray(interfaceToValue(item))
		}
		return arr
	case map[string]interface{}:
		obj := pln.NewObject()
		for k, item := range val {
			obj.AddToObject(k, interfaceToValue(item))
		}
		return obj
	}
	return pln.NewString(fmt.Sprintf("%v", v))
}
