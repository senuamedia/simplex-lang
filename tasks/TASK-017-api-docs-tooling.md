# TASK-017: API Documentation System and Developer Tooling

**Status**: Mostly Complete (~85%)
**Priority**: High (Developer Experience)
**Target Version**: 0.10.0
**Updated**: 2026-03-16 (audited against codebase)

> **Audit Note (2026-03-16):** Core documentation system implemented:
> - `sxdoc-index.sx` (800 lines): manifest generation, json_escape(), search index
> - `simplex-docs/api/`: index.html, search-index.json (703KB), api-context.json, 22+ module manifests
> - Assets: docs.css, search.js
> - CI: documentation build job in `.github/workflows/build.yml`
>
> Missing: sxdoc.sx `--manifest` and `--category` flags not implemented
**Codebase**: `/Users/rod/code/simplex`
**Depends On**:
- Phase 1 (Core complete)
- sxdoc.sx (existing)

> **Vision**: A comprehensive, AI-friendly API documentation system with dynamic navigation, structured metadata, and seamless GitHub integration. All tooling written in Simplex.

---

## Overview

Build a professional API documentation system that serves both human developers and AI/LLM consumers. The system generates searchable, navigable HTML documentation with JSON-LD metadata, manifest files for tooling, and a unified index.

---

## Deliverables

### 1. Enhanced sxdoc.sx

| Feature | Description | Status |
|---------|-------------|--------|
| `json_escape()` | JSON string escaping function | In Progress |
| `generate_jsonld()` | Generate JSON-LD structured data | In Progress |
| `--category` flag | Set module category for organization | Pending |
| `--manifest` flag | Output manifest.json with all items | Pending |
| Enhanced HTML template | Viewport, external CSS, breadcrumbs | Pending |

### 2. sxdoc-index.sx (New Tool)

New Simplex tool that reads manifest files and generates:
- `index.html` - Dynamic navigation wrapper with sidebar
- `search-index.json` - Client-side search data
- `api-context.json` - AI/LLM consumption context

### 3. Build Orchestration

| File | Purpose |
|------|---------|
| `scripts/build-docs.sh` | Orchestrate full documentation build |

### 4. Assets

| File | Purpose |
|------|---------|
| `simplex-docs/api/assets/docs.css` | Documentation styles |
| `simplex-docs/api/assets/search.js` | Client-side search functionality |

### 5. GitHub Integration

Add documentation build job to `.github/workflows/build.yml`:
- Build documentation after successful Linux build
- Upload as artifact for deployment

---

## Technical Design

### JSON-LD Schema

Each generated HTML file includes structured data:

```json
{
  "@context": "https://schema.org",
  "@type": "TechArticle",
  "headline": "math - Simplex API Reference",
  "description": "Mathematical functions for the Simplex language",
  "programmingLanguage": {
    "@type": "ComputerLanguage",
    "name": "Simplex",
    "url": "https://github.com/senuamedia/simplex"
  },
  "isPartOf": {
    "@type": "WebSite",
    "name": "Simplex Documentation",
    "url": "https://simplex.senuamedia.com/api/"
  },
  "articleSection": "std"
}
```

### Manifest Format

Each category generates `manifest.json`:

```json
{
  "category": "std",
  "modules": [
    {
      "name": "math",
      "file": "math.html",
      "items": [
        {"kind": "fn", "name": "sqrt", "signature": "fn sqrt(x: f64) -> f64"},
        {"kind": "fn", "name": "sin", "signature": "fn sin(x: f64) -> f64"}
      ]
    }
  ]
}
```

### API Context Format

`api-context.json` for AI consumption:

```json
{
  "name": "Simplex API",
  "version": "0.10.0",
  "categories": {
    "std": {
      "description": "Standard library",
      "modules": ["math", "io", "string", "collections"]
    },
    "json": {
      "description": "JSON parsing and generation",
      "modules": ["lib"]
    }
  },
  "totalFunctions": 450,
  "totalStructs": 85,
  "baseUrl": "https://simplex.senuamedia.com/api/"
}
```

### Directory Structure

```
simplex-docs/api/
├── index.html              # Generated navigation wrapper
├── api-context.json        # AI context file
├── search-index.json       # Search data
├── assets/
│   ├── docs.css            # Styles
│   └── search.js           # Search functionality
├── std/
│   ├── manifest.json       # Category manifest
│   ├── math.html
│   ├── io.html
│   └── ...
├── json/
│   ├── manifest.json
│   └── lib.html
└── ... (other categories)
```

---

## Module Categories

| Category | Source Directory | Description |
|----------|-----------------|-------------|
| std | simplex-std/src | Standard library |
| json | simplex-json/src | JSON parsing |
| sql | simplex-sql/src | SQLite bindings |
| toml | simplex-toml/src | TOML parsing |
| uuid | simplex-uuid/src | UUID generation |
| s3 | simplex-s3/src | AWS S3 client |
| ses | simplex-ses/src | AWS SES client |
| inference | simplex-inference/src | ML inference |
| learning | simplex-learning/src | ML training |
| simplex-edge-hive | simplex-edge-hive/src | Edge computing |
| simplex-nexus | simplex-nexus/src | Network protocol |
| lib | lib | Core library |
| runtime | runtime | Runtime functions |

---

## Implementation Steps

### Phase 1: sxdoc.sx Enhancements

1. Add `json_escape()` function
2. Add `generate_jsonld()` function
3. Update `generate_html()` with:
   - JSON-LD in `<head>`
   - Viewport meta tag
   - Link to external CSS
   - Breadcrumb navigation
4. Add `--category` flag parsing
5. Add `--manifest` flag and manifest generation

### Phase 2: sxdoc-index.sx

1. Create new tool with argument parsing
2. Implement `scan_manifests()` - find all manifest.json files
3. Implement `parse_manifest()` - parse JSON manifest
4. Implement `generate_index_html()` - create navigation page
5. Implement `generate_search_index()` - create search JSON
6. Implement `generate_api_context()` - create AI context

### Phase 3: Assets and Build

1. Create `docs.css` with sidebar layout
2. Create `search.js` with client-side search
3. Create `build-docs.sh` orchestration script
4. Update GitHub workflow

---

## Future Tooling Roadmap

### Debugger (sxdb)

| Feature | Description | Priority |
|---------|-------------|----------|
| Breakpoints | Set breakpoints in source | High |
| Step execution | Step in/over/out | High |
| Variable inspection | View current values | High |
| Stack traces | Call stack display | Medium |
| Watch expressions | Monitor expressions | Medium |

### Syntax Parser Improvements

| Feature | Description | Priority |
|---------|-------------|----------|
| Error recovery | Continue parsing after errors | High |
| Better spans | Accurate source locations | High |
| Incremental parsing | Parse only changed code | Medium |

### IDE Tooling

| Feature | Description | Priority |
|---------|-------------|----------|
| VS Code extension | Syntax highlighting, snippets | Exists |
| sxlsp improvements | Better completions, hover | High |
| Go to definition | Jump to symbol definition | High |
| Find references | Find all usages | Medium |
| Refactoring | Rename symbol | Medium |

### Package Manager (sxpm)

| Feature | Description | Priority |
|---------|-------------|----------|
| Dependency resolution | Resolve version conflicts | Exists |
| Registry | Central package registry | Medium |
| Lock files | Reproducible builds | Medium |

---

## Validation Criteria

### Documentation System

- [ ] `sxdoc --manifest` generates valid manifest.json
- [ ] `sxdoc --category std` sets category in JSON-LD
- [ ] JSON-LD validates with Google Rich Results Test
- [ ] `sxdoc-index` generates working index.html
- [ ] Search finds functions across modules
- [ ] `api-context.json` is valid JSON

### Build Integration

- [ ] `./scripts/build-docs.sh` completes successfully
- [ ] All categories generate documentation
- [ ] GitHub workflow builds and uploads docs
- [ ] Documentation artifact downloadable

### User Experience

- [ ] Index page loads with navigation
- [ ] Search returns relevant results
- [ ] Links between modules work
- [ ] Responsive design works on mobile

---

## References

- [Schema.org TechArticle](https://schema.org/TechArticle)
- [JSON-LD](https://json-ld.org/)
- [Google Structured Data](https://developers.google.com/search/docs/appearance/structured-data)
- Existing: `sxdoc.sx`, `simplex-docs/api/`
