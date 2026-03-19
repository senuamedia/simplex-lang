# TASK-023: Data Format Libraries

**Status**: Complete
**Priority**: High
**Created**: 2026-03-16
**Target Version**: 0.16.0
**Depends On**: TASK-022 Phase 8 (JSON parser complete)

---

## Overview

Standard data format libraries for interchange, configuration, and enterprise integration. These are table-stakes for any language competing for real-world adoption. All implementations must be **pure Simplex** — no external C libraries or bindings beyond what the runtime already provides.

---

## Library 1: simplex-csv

**Location**: `simplex-std/src/csv.sx`
**Priority**: Critical — universal data interchange format

### Core API
```simplex
struct CsvConfig {
    delimiter: String,      // Default ","
    quote_char: String,     // Default "\""
    has_header: bool,       // Default true
    skip_empty: bool        // Default true
}

fn csv_parse(input: String, config: CsvConfig) -> Result<CsvTable, CsvError>
fn csv_parse_default(input: String) -> Result<CsvTable, CsvError>
fn csv_stringify(table: CsvTable, config: CsvConfig) -> String

struct CsvTable {
    headers: Vec<String>,
    rows: Vec<Vec<String>>
}
```

### Features
- RFC 4180 compliant parsing
- Configurable delimiter (CSV, TSV, pipe-separated)
- Quoted field handling (embedded commas, newlines, quotes)
- Header row detection
- Row-by-row streaming for large files (iterator-based)
- Type coercion helpers: `csv_column_as_f64()`, `csv_column_as_i64()`

### Success Criteria
- Round-trip: `csv_parse(csv_stringify(table)) = table`
- Handles quoted fields with embedded delimiters and newlines
- Streaming parse of 100K+ row file without loading entire file into memory
- New test: `tests/stdlib/spec_csv.sx`

---

## Library 2: simplex-yaml

**Location**: `simplex-std/src/yaml.sx`
**Priority**: High — config files, K8s manifests, CI/CD pipelines

### Core API
```simplex
enum YamlValue {
    Mapping(Vec<YamlEntry>),
    Sequence(Vec<YamlValue>),
    Scalar(String),
    Null
}

struct YamlEntry {
    key: String,
    value: YamlValue
}

fn yaml_parse(input: String) -> Result<YamlValue, YamlError>
fn yaml_stringify(value: YamlValue) -> String
fn yaml_stringify_pretty(value: YamlValue, indent: i64) -> String
```

### Features
- YAML 1.2 Core Schema subset (mappings, sequences, scalars, null)
- Indentation-based nesting detection
- Multi-document support (`---` separator)
- Anchor/alias references (`&anchor`, `*alias`)
- Flow style parsing (`{key: value}`, `[1, 2, 3]`)
- Type inference: unquoted `true`/`false` -> bool, numeric strings -> numbers
- Comment preservation (store but don't interpret `#` comments)

### Out of Scope (v1)
- Custom tags (`!!binary`, `!!timestamp`)
- Complex merge keys (`<<`)
- YAML 1.1 compatibility quirks

### Success Criteria
- Parses standard Kubernetes manifest YAML
- Parses GitHub Actions workflow YAML
- Round-trip: `yaml_parse(yaml_stringify(value)) = value` for core types
- Handles multi-line strings (literal `|` and folded `>` block scalars)
- New test: `tests/stdlib/spec_yaml.sx`

---

## Library 3: simplex-xml

**Location**: `simplex-std/src/xml.sx`
**Priority**: Medium — enterprise integrations, SOAP, RSS/Atom, SVG

### Core API
```simplex
enum XmlNode {
    Element(XmlElement),
    Text(String),
    CData(String),
    Comment(String)
}

struct XmlElement {
    tag: String,
    attributes: Vec<XmlAttr>,
    children: Vec<XmlNode>
}

struct XmlAttr {
    name: String,
    value: String
}

fn xml_parse(input: String) -> Result<XmlNode, XmlError>
fn xml_stringify(node: XmlNode) -> String
fn xml_stringify_pretty(node: XmlNode, indent: i64) -> String
```

### Features
- Well-formed XML parsing (not full DTD/schema validation)
- Namespace awareness (prefix:local parsing, xmlns attributes)
- CDATA sections
- Entity references (`&amp;`, `&lt;`, `&gt;`, `&quot;`, `&apos;`)
- XPath-lite query: `xml_find(node, "path/to/element")` -> `Vec<XmlNode>`
- Attribute access helpers

### Out of Scope (v1)
- DTD validation
- XML Schema (XSD) validation
- XSLT transformation
- Full XPath 2.0+

### Success Criteria
- Parses RSS 2.0 feeds correctly
- Parses SOAP envelopes correctly
- Handles namespaces without data loss
- Entity encoding/decoding round-trips correctly
- New test: `tests/stdlib/spec_xml.sx`

---

## Library 4: simplex-toml (Enhancement)

**Location**: `simplex-std/src/toml.sx` (exists — extend if needed)
**Priority**: Low — already exists, may need polish

### Review & Enhancement
- Audit existing implementation for TOML v1.0 compliance
- Add missing features if any (inline tables, array of tables, datetime)
- Ensure round-trip fidelity

---

## Dependency Graph

```
TASK-022 Phase 8 (JSON parser)
    |
    v
simplex-csv (independent)
simplex-yaml (independent)
simplex-xml (independent)
simplex-toml (review only)
```

All four libraries are independent of each other and can be built in parallel.

---

## Estimated Line Counts

| Library | Est. Lines |
|---------|-----------|
| simplex-csv | ~600-800 |
| simplex-yaml | ~1,200-1,600 |
| simplex-xml | ~1,000-1,400 |
| simplex-toml | ~200 (review/fixes) |
| **Total** | **~3,000-4,000** |
