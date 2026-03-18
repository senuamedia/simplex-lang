# Simplex Development Roadmap

This directory contains the phased development plan for completing the Simplex language ecosystem.

## Current Status

| Phase | Name | Progress | Status | Priority |
|-------|------|----------|--------|----------|
| 1 | [Complete Core](phase1-complete-core.md) | [**0/137**](phase1-progress.md) | Not Started | CRITICAL |
| 2 | [Package Ecosystem](phase2-package-ecosystem.md) | [**0/62**](phase2-progress.md) | Not Started | HIGH |
| 3 | [Essential Libraries](phase3-essential-libraries.md) | [**0/114**](phase3-progress.md) | Not Started | HIGH |
| 4 | [The Anima - Cognitive Soul](phase4-ai-actor-unique-value.md) | [**0/112**](phase4-progress.md) | Not Started | HIGH |

**Total Tasks**: 425
**Completed**: 0 (0%)

---

## Phase Dependencies

```
Phase 1: Complete Core
    │
    ├── HashMap, HashSet
    ├── String operations
    ├── Iterator combinators
    ├── Result/Option methods
    └── JSON serialization
           │
           ▼
Phase 2: Package Ecosystem
    │
    ├── Package manifest (TOML/JSON)
    ├── Cursus integration
    ├── Dependency resolution
    └── Package registry
           │
           ▼
Phase 3: Essential Libraries
    │
    ├── simplex-json
    ├── simplex-http
    ├── simplex-sql
    ├── simplex-regex
    ├── simplex-crypto
    ├── simplex-cli
    ├── simplex-log
    ├── simplex-test
    ├── simplex-toml
    └── simplex-uuid
           │
           ▼
Phase 4: The Anima - Cognitive Soul
    │
    ├── The Anima construct (memory, beliefs, desires, intentions, identity)
    ├── Cognitive memory (episodic, semantic, procedural, working)
    ├── Belief system with revision and evidence
    ├── Native SLM (simplex-anima) - ships with runtime
    ├── Tool/function calling
    ├── Multi-actor orchestration
    ├── Specialist/Hive enhancements
    ├── Actor-Anima integration
    └── Observability
```

---

## Strategic Principles

### 1. Don't Chase Python's 3000+ Libraries
Focus on quality over quantity. Build 10 excellent libraries, not 100 mediocre ones.

### 2. Double Down on Differentiators
Simplex is unique because of:
- **Actors**: Erlang-style fault tolerance built-in
- **AI-native**: First-class specialists, hives, inference
- **Systems + AI**: Neither Rust nor Python does both well

### 3. Complete the Foundation First
Everything depends on Phase 1. Don't skip ahead.

### 4. Enable the Community
Phase 2 (package ecosystem) enables others to contribute libraries.

---

## Success Metrics

### Phase 1 Complete When:
- [ ] HashMap/HashSet fully working with tests
- [ ] All string operations available
- [ ] Iterator combinators with method chaining
- [ ] Option/Result full method suite
- [ ] JSON parse AND serialize

### Phase 2 Complete When:
- [ ] `cursus new` creates valid packages
- [ ] `cursus build` handles dependencies
- [ ] `cursus publish` uploads to registry
- [ ] Lock files ensure reproducibility
- [ ] 5+ packages published

### Phase 3 Complete When:
- [ ] 10 libraries published
- [ ] Each has tests and docs
- [ ] 2+ example applications

### Phase 4 Complete When:
- [ ] `anima` keyword implemented in parser/lexer/codegen
- [ ] Cognitive memory system working (episodic, semantic, procedural, working)
- [ ] Belief system with revision and contradiction detection
- [ ] Native SLM (simplex-anima) ships with runtime
- [ ] Tool calling with auto schema generation
- [ ] Actor-Anima integration seamless
- [ ] Complete AI assistant example with persistent anima

---

## Quick Start

To start working on a phase:

```bash
# Read the phase document
cat tasks/phase1-complete-core.md

# Track progress by checking off items
# Edit the markdown files directly

# When a task is complete, update the status
```

---

## Files in This Directory

| File | Description |
|------|-------------|
| `README.md` | This file - overview and index |
| **Task Definitions** | |
| `phase1-complete-core.md` | Core language features |
| `phase2-package-ecosystem.md` | Package manager and registry |
| `phase3-essential-libraries.md` | 10 essential libraries |
| `phase4-ai-actor-unique-value.md` | The Anima - Cognitive Soul |
| **Progress Tracking** | |
| `phase1-progress.md` | Phase 1 task checklist (137 tasks) |
| `phase2-progress.md` | Phase 2 task checklist (62 tasks) |
| `phase3-progress.md` | Phase 3 task checklist (114 tasks) |
| `phase4-progress.md` | Phase 4 task checklist (112 tasks) |
