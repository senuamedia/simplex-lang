# TASK-052: Simplex Training Data Pipeline

**Version:** 0.18.0
**Status:** Planned
**Priority:** P0 — Critical (Must Be First)
**Depends on:** v0.17.0 release

## Why This Feature Is Needed

You cannot build a model without data. The entire v0.18.0 release depends on having a
high-quality, well-structured training corpus derived from the Simplex ecosystem itself.
This is not web-scraped general text — it is curated, domain-specific data that teaches
a model what Simplex *is*: the syntax, the semantics, the idioms, the patterns, the
error modes, the fixes, the architecture decisions, and the reasoning behind them.

No existing dataset teaches a model about Simplex. The language is unique — its neural
gates, cognitive hives, dual numbers, belief systems, and contract logic exist nowhere
else. Every token of training data must come from the Simplex ecosystem itself, or be
synthetically generated to cover gaps.

The training data pipeline extracts, transforms, validates, and packages data from every
source in the Simplex project into training-ready formats. It is the foundation on which
every model in v0.18.0 is built.

## Why It Adds Value

1. **Single source of truth.** One pipeline produces all training data for all
   specialists. Consistency across specialists means they share a coherent understanding
   of Simplex.

2. **Quality over quantity.** A small, perfectly curated dataset beats a massive noisy
   one for specialist models. The pipeline includes validation, deduplication, and
   quality scoring.

3. **Synthetic data generation.** For patterns that are underrepresented in the existing
   codebase (error handling, edge cases, complex generics), the pipeline generates
   synthetic examples using template-based and grammar-based generation.

4. **Reproducibility.** Every training run can trace back to exactly which data it used.
   Data provenance is tracked end-to-end.

5. **Continuous updates.** As Simplex evolves (new features, new tests, new patterns),
   the pipeline regenerates updated training data. The models stay current with the
   language.

## Why It Changes Systems Built With Simplex

Without this pipeline, there are no models. With it, every future model inherits a
deep, structured understanding of Simplex from its training data.

## Deliverables

### Phase 1: Source Extraction (~500 lines)

Location: `simplex-training/src/data/`

- **CodeExtractor** — parse all .sx files into structured records: function signatures,
  implementations, docstrings, imports, struct definitions, trait implementations,
  neural gate definitions, contract annotations
- **SpecExtractor** — parse language specification markdown into structured knowledge:
  syntax rules, semantic descriptions, examples, constraints
- **TestExtractor** — parse test files into input/expected-output pairs, categorized by
  feature area
- **DocExtractor** — parse tutorials, guides, and API docs into instruction/explanation
  pairs
- **ErrorExtractor** — extract compiler error messages, their causes, and their fixes
  from git history and error code documentation
- **RuntimeExtractor** — parse the C runtime into function signatures, doc comments,
  and behavioral descriptions

```simplex
/// Extract structured training data from Simplex source files
struct CodeExtractor {
    parser: Parser,
}

/// A single training record from source code
struct CodeRecord {
    source_file: String,
    record_type: CodeRecordType,
    content: String,
    context: String,        // surrounding code for context
    metadata: RecordMetadata,
}

enum CodeRecordType {
    FunctionDef { name: String, params: Vec<String>, return_type: String, body: String },
    StructDef { name: String, fields: Vec<String>, methods: Vec<String> },
    TraitDef { name: String, methods: Vec<String> },
    NeuralGate { name: String, contracts: Vec<String>, body: String },
    TestCase { name: String, assertions: Vec<String>, feature_area: String },
    Import { modules: Vec<String> },
}

impl CodeExtractor {
    /// Extract all records from a .sx file
    fn extract_file(self: &Self, path: &str) -> Vec<CodeRecord> {
        let source = read_file(path);
        let ast = self.parser.parse(&source);
        let mut records = Vec::new();

        for node in ast.walk() {
            match node {
                AstNode::Function(f) => {
                    records.push(CodeRecord {
                        source_file: path.to_string(),
                        record_type: CodeRecordType::FunctionDef {
                            name: f.name.clone(),
                            params: f.params.iter().map(|p| p.to_string()).collect(),
                            return_type: f.return_type.to_string(),
                            body: f.body.to_string(),
                        },
                        content: f.to_source(),
                        context: self.extract_context(&source, f.span),
                        metadata: RecordMetadata::from_file(path, f.span),
                    });
                },
                // ... similar for structs, traits, neural gates, tests
                _ => {}
            }
        }
        records
    }

    /// Extract all records from entire codebase
    fn extract_codebase(self: &Self, root: &str) -> Vec<CodeRecord> {
        glob(root, "**/*.sx").iter()
            .flat_map(|path| self.extract_file(path))
            .collect()
    }
}
```

Files:
- `simplex-training/src/data/code_extractor.sx` — CodeExtractor
- `simplex-training/src/data/spec_extractor.sx` — SpecExtractor
- `simplex-training/src/data/test_extractor.sx` — TestExtractor
- `simplex-training/src/data/doc_extractor.sx` — DocExtractor
- `simplex-training/src/data/error_extractor.sx` — ErrorExtractor
- `simplex-training/src/data/runtime_extractor.sx` — RuntimeExtractor

### Phase 2: Data Transformation & Augmentation (~500 lines)

- **TaskFormatter** — transform raw records into task-specific formats:
  - Code completion: `prefix → suffix`
  - Code generation: `description → implementation`
  - Error explanation: `error message → explanation + fix`
  - Test generation: `function signature → test cases`
  - Documentation: `code → docstring`
  - Refactoring: `code_v1 → code_v2` (from git history)
- **SyntheticGenerator** — generate additional training examples for underrepresented
  patterns using grammar-based generation and template instantiation
- **Deduplicator** — remove duplicate or near-duplicate records using MinHash
- **QualityScorer** — score each record on completeness, correctness, and informativeness

```simplex
/// Format records into task-specific training pairs
struct TaskFormatter {
    task_type: TaskType,
}

enum TaskType {
    CodeCompletion,    // given prefix, predict continuation
    CodeGeneration,    // given description, generate code
    ErrorExplanation,  // given error, explain and fix
    TestGeneration,    // given function, generate tests
    Documentation,     // given code, generate docs
    Refactoring,       // given old code, generate improved version
    ArchitectureQA,    // given architecture question, answer with Simplex patterns
}

/// A training pair ready for model consumption
struct TrainingPair {
    input: String,
    output: String,
    task: TaskType,
    quality_score: f64,
    provenance: String,
}

impl TaskFormatter {
    fn format(self: &Self, record: &CodeRecord) -> Vec<TrainingPair> {
        match self.task_type {
            TaskType::CodeCompletion => {
                // Split function body at multiple points for completion pairs
                let body = &record.content;
                let split_points = find_statement_boundaries(body);
                split_points.iter().map(|&split| {
                    TrainingPair {
                        input: body[..split].to_string(),
                        output: body[split..].to_string(),
                        task: TaskType::CodeCompletion,
                        quality_score: 1.0,
                        provenance: record.source_file.clone(),
                    }
                }).collect()
            },
            TaskType::TestGeneration => {
                // Input: function signature, Output: test cases that exercise it
                if let CodeRecordType::FunctionDef { name, params, return_type, .. } = &record.record_type {
                    vec![TrainingPair {
                        input: format!("fn {}({}) -> {}", name,
                            params.join(", "), return_type),
                        output: self.find_tests_for(name),
                        task: TaskType::TestGeneration,
                        quality_score: 1.0,
                        provenance: record.source_file.clone(),
                    }]
                } else { vec![] }
            },
            // ... other task types
            _ => vec![],
        }
    }
}
```

Files:
- `simplex-training/src/data/formatter.sx` — TaskFormatter
- `simplex-training/src/data/synthetic.sx` — SyntheticGenerator
- `simplex-training/src/data/dedup.sx` — Deduplicator (MinHash)
- `simplex-training/src/data/quality.sx` — QualityScorer

### Phase 3: Tokenization & Packaging (~400 lines)

- **SimplexTokenizer** — BPE tokenizer trained specifically on Simplex source code,
  with special tokens for Simplex keywords, neural gate annotations, and contract
  keywords
- **DatasetBuilder** — package training pairs into sharded, shuffled datasets with
  train/validation/test splits
- **DatasetStats** — compute and report dataset statistics: token count, vocabulary
  coverage, task distribution, quality distribution
- **DataLoader** — efficient streaming data loader for training with prefetching and
  batching

Files:
- `simplex-training/src/data/tokenizer.sx` — SimplexTokenizer (BPE)
- `simplex-training/src/data/dataset.sx` — DatasetBuilder
- `simplex-training/src/data/stats.sx` — DatasetStats
- `simplex-training/src/data/dataloader.sx` — streaming DataLoader

### Phase 4: Tests (~300 lines)

Location: `tests/training_data/`

- CodeExtractor correctly parses all function definitions in simplex-std
- TaskFormatter produces valid completion pairs (prefix + output = original)
- SyntheticGenerator produces parseable Simplex code
- Deduplicator removes exact and near-duplicates
- SimplexTokenizer round-trips Simplex source code without loss
- DatasetBuilder produces balanced train/val/test splits

## Success Criteria

- [ ] Extract >5,000 structured records from existing Simplex codebase
- [ ] Generate >10,000 training pairs across all task types
- [ ] Synthetic generator produces code that parses without errors
- [ ] Tokenizer achieves >95% vocabulary coverage on Simplex source
- [ ] Dataset splits maintain task-type distribution within 5%
- [ ] Full pipeline runs end-to-end on current codebase in <60 seconds

## Estimated Scope

~1,700 lines across library code and tests.
