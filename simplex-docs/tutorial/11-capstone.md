# Chapter 11: Building a Complete Project

In this final chapter, we'll build a complete application that combines everything you've learned: types, functions, actors, supervision, and AI integration.

---

## Project: Smart Document Processor

We'll build a document processing service that:
1. Ingests documents from multiple sources
2. Analyzes them with AI (summarization, entity extraction, sentiment)
3. Stores results with fault tolerance
4. Provides semantic search

This is a real-world pattern used in enterprise applications.

---

## Project Structure

```
document-processor/
├── simplex.toml
├── src/
│   ├── main.sx
│   ├── types.sx
│   ├── actors/
│   │   ├── ingester.sx
│   │   ├── processor.sx
│   │   ├── storage.sx
│   │   └── api.sx
│   └── supervisors/
│       └── app.sx
└── tests/
    └── processor_test.sx
```

---

## Step 1: Define Types

First, let's define our data types in `src/types.sx`:

```simplex
// src/types.sx
module types

// Unique identifiers
pub type DocumentId = String
pub type UserId = String

// Source of document
pub enum Source {
    Upload,
    Email,
    Api,
    Webhook
}

// Raw document input
pub type Document {
    id: DocumentId,
    source: Source,
    content: String,
    metadata: Map<String, String>,
    received_at: Instant
}

impl Document {
    pub fn new(source: Source, content: String) -> Document {
        Document {
            id: uuid::v4(),
            source,
            content,
            metadata: {},
            received_at: Instant::now()
        }
    }
}

// Extracted entity
pub type Entity {
    text: String,
    entity_type: EntityType,
    confidence: f64
}

pub enum EntityType {
    Person,
    Organization,
    Location,
    Date,
    Money,
    Product,
    Other
}

// Sentiment classification
pub enum Sentiment {
    Positive,
    Negative,
    Neutral,
    Mixed
}

// Fully processed document
pub type ProcessedDocument {
    id: DocumentId,
    original: Document,
    summary: String,
    entities: List<Entity>,
    sentiment: Sentiment,
    topics: List<String>,
    embedding: Vector<f64, 1536>,
    processed_at: Instant,
    processing_time_ms: i64
}

// Search result
pub type SearchResult {
    document: ProcessedDocument,
    score: f64,
    highlights: List<String>
}

// Statistics
pub type Stats {
    documents_received: i64,
    documents_processed: i64,
    documents_failed: i64,
    avg_processing_time_ms: f64
}
```

---

## Step 2: Document Ingester

The ingester receives documents and queues them for processing.

```simplex
// src/actors/ingester.sx
module actors::ingester

use types::*

pub actor DocumentIngester {
    var queue: List<Document> = []
    var processor_pool: Option<ActorRef<ProcessorPool>> = None
    var stats: Stats = Stats::default()

    receive SetProcessor(pool: ActorRef<ProcessorPool>) {
        processor_pool = Some(pool)

        // Process any queued documents
        for doc in queue {
            send(pool, Process(doc))
        }
        queue = []
    }

    receive Ingest(source: Source, content: String) -> DocumentId {
        let doc = Document::new(source, content)
        stats.documents_received += 1

        match processor_pool {
            Some(pool) => {
                send(pool, Process(doc.clone()))
            },
            None => {
                // Queue until processor is ready
                queue.push_mut(doc.clone())
            }
        }

        checkpoint()
        doc.id
    }

    receive GetStats -> Stats {
        stats.clone()
    }

    on_resume() {
        log::info("Ingester resumed with {queue.len()} queued documents")
    }
}

impl Stats {
    fn default() -> Stats {
        Stats {
            documents_received: 0,
            documents_processed: 0,
            documents_failed: 0,
            avg_processing_time_ms: 0.0
        }
    }
}
```

---

## Step 3: Document Processor

The processor uses AI to analyze documents.

```simplex
// src/actors/processor.sx
module actors::processor

use types::*
use ai

pub actor DocumentProcessor {
    var id: i64
    var storage: ActorRef<DocumentStorage>
    var processing: Option<Document> = None

    init(processor_id: i64, storage_ref: ActorRef<DocumentStorage>) {
        id = processor_id
        storage = storage_ref
    }

    receive Process(doc: Document) {
        let start = Instant::now()

        // Track in-flight work for recovery
        processing = Some(doc.clone())
        checkpoint()

        log::info("Processor {id} analyzing document {doc.id}")

        // Run AI operations in parallel
        let (summary, entities, sentiment, topics, embedding) = await parallel(
            summarize(doc.content),
            extract_entities(doc.content),
            classify_sentiment(doc.content),
            extract_topics(doc.content),
            ai::embed(doc.content)
        )

        let processing_time = start.elapsed().as_millis()

        let processed = ProcessedDocument {
            id: doc.id,
            original: doc,
            summary,
            entities,
            sentiment,
            topics,
            embedding,
            processed_at: Instant::now(),
            processing_time_ms: processing_time
        }

        // Send to storage
        send(storage, Store(processed))

        processing = None
        checkpoint()

        log::info("Processor {id} completed document {doc.id} in {processing_time}ms")
    }

    on_resume() {
        // Retry any in-flight document
        if let Some(doc) = processing {
            log::warn("Processor {id} resuming interrupted document {doc.id}")
            send(self, Process(doc))
        }
    }
}

// Helper functions
async fn summarize(content: String) -> String {
    await ai::complete(
        "Summarize the following text in 2-3 sentences:\n\n{content}",
        max_tokens: 150
    )
}

async fn extract_entities(content: String) -> List<Entity> {
    await ai::extract<List<Entity>>(content)
}

async fn classify_sentiment(content: String) -> Sentiment {
    await ai::classify<Sentiment>(content)
}

async fn extract_topics(content: String) -> List<String> {
    await ai::extract<List<String>>(
        "Extract the main topics from this text as a list: {content}"
    )
}

// Processor pool for load balancing
pub actor ProcessorPool {
    var processors: List<ActorRef<DocumentProcessor>> = []
    var next: i64 = 0

    init(size: i64, storage: ActorRef<DocumentStorage>) {
        for i in 0..size {
            processors.push_mut(spawn DocumentProcessor(i, storage))
        }
        log::info("Processor pool started with {size} workers")
    }

    receive Process(doc: Document) {
        // Round-robin distribution
        let processor = processors[next % processors.len()]
        next += 1
        send(processor, Process(doc))
    }
}
```

---

## Step 4: Document Storage

Storage persists documents and enables search.

```simplex
// src/actors/storage.sx
module actors::storage

use types::*
use ai

pub actor DocumentStorage {
    var documents: Map<DocumentId, ProcessedDocument> = {}
    var embeddings: Map<DocumentId, Vector<f64, 1536>> = {}
    var stats: StorageStats = StorageStats::default()

    receive Store(doc: ProcessedDocument) {
        documents.insert(doc.id.clone(), doc.clone())
        embeddings.insert(doc.id.clone(), doc.embedding.clone())

        stats.total_documents += 1
        stats.total_entities += doc.entities.len()

        checkpoint()

        log::info("Stored document {doc.id}")
    }

    receive Get(id: DocumentId) -> Option<ProcessedDocument> {
        documents.get(id)
    }

    receive Search(query: String, limit: i64) -> List<SearchResult> {
        if documents.is_empty() {
            return []
        }

        let query_embedding = ai::embed(query)

        // Find similar documents
        let embedding_list: List<Vector<f64, 1536>> = embeddings.values().collect()
        let id_list: List<DocumentId> = embeddings.keys().collect()

        let matches = ai::nearest(query_embedding, embedding_list, k: limit)

        matches
            .filter_map((idx, score) => {
                let id = id_list[idx]
                documents.get(id).map(doc => SearchResult {
                    document: doc,
                    score,
                    highlights: find_highlights(query, doc.original.content)
                })
            })
            .collect()
    }

    receive GetByTopic(topic: String) -> List<ProcessedDocument> {
        documents.values()
            .filter(doc => doc.topics.any(t => t.contains(topic)))
            .collect()
    }

    receive GetBySentiment(sentiment: Sentiment) -> List<ProcessedDocument> {
        documents.values()
            .filter(doc => doc.sentiment == sentiment)
            .collect()
    }

    receive GetStats -> StorageStats {
        stats.clone()
    }
}

type StorageStats {
    total_documents: i64,
    total_entities: i64
}

impl StorageStats {
    fn default() -> StorageStats {
        StorageStats {
            total_documents: 0,
            total_entities: 0
        }
    }
}

fn find_highlights(query: String, content: String) -> List<String> {
    // Simple highlight extraction
    let words = query.split_whitespace()
    content.lines()
        .filter(line => words.any(w => line.contains(w)))
        .take(3)
        .collect()
}
```

---

## Step 5: API Actor

The API provides external interface to the system.

```simplex
// src/actors/api.sx
module actors::api

use types::*

pub actor DocumentApi {
    var ingester: ActorRef<DocumentIngester>
    var storage: ActorRef<DocumentStorage>

    init(ing: ActorRef<DocumentIngester>, stor: ActorRef<DocumentStorage>) {
        ingester = ing
        storage = stor
    }

    // Ingest a new document
    receive IngestDocument(source: Source, content: String) -> DocumentId {
        ask(ingester, Ingest(source, content))
    }

    // Search documents
    receive Search(query: String, limit: i64 = 10) -> List<SearchResult> {
        ask(storage, Search(query, limit))
    }

    // Get specific document
    receive GetDocument(id: DocumentId) -> Option<ProcessedDocument> {
        ask(storage, Get(id))
    }

    // Get documents by topic
    receive GetByTopic(topic: String) -> List<ProcessedDocument> {
        ask(storage, GetByTopic(topic))
    }

    // Get system statistics
    receive GetSystemStats -> SystemStats {
        let ingester_stats = ask(ingester, GetStats)
        let storage_stats = ask(storage, GetStats)

        SystemStats {
            ingested: ingester_stats.documents_received,
            processed: storage_stats.total_documents,
            entities_extracted: storage_stats.total_entities
        }
    }
}

type SystemStats {
    ingested: i64,
    processed: i64,
    entities_extracted: i64
}
```

---

## Step 6: Supervision Tree

Set up fault-tolerant supervision.

```simplex
// src/supervisors/app.sx
module supervisors::app

use actors::*
use types::*

supervisor StorageLayer {
    strategy: OneForOne,
    max_restarts: 10,
    within: Duration::minutes(1),

    children: [
        child(DocumentStorage, name: "storage", restart: Always)
    ]
}

supervisor ProcessingLayer {
    strategy: OneForOne,
    max_restarts: 20,
    within: Duration::minutes(1),

    children: [
        child(ProcessorPool(size: 4, storage: sibling("storage")),
              name: "processors",
              restart: Always)
    ]
}

supervisor IngestionLayer {
    strategy: OneForOne,
    max_restarts: 10,
    within: Duration::minutes(1),

    children: [
        child(DocumentIngester, name: "ingester", restart: Always)
    ]
}

supervisor Application {
    strategy: RestForOne,  // If storage fails, restart processing and ingestion
    max_restarts: 5,
    within: Duration::minutes(5),

    children: [
        child(StorageLayer, name: "storage_layer"),
        child(ProcessingLayer, name: "processing_layer"),
        child(IngestionLayer, name: "ingestion_layer")
    ]

    on_start() {
        // Wire up the components
        let storage = child("storage_layer").child("storage")
        let processors = child("processing_layer").child("processors")
        let ingester = child("ingestion_layer").child("ingester")

        send(ingester, SetProcessor(processors))

        log::info("Application started")
    }
}
```

---

## Step 7: Main Entry Point

```simplex
// src/main.sx
module main

use supervisors::app::Application
use actors::api::DocumentApi
use types::*

fn main() {
    log::info("Starting Document Processor...")

    // Start the application
    let app = spawn Application

    // Create API endpoint
    let storage = app.child("storage_layer").child("storage")
    let ingester = app.child("ingestion_layer").child("ingester")
    let api = spawn DocumentApi(ingester, storage)

    log::info("System ready. Ingesting sample documents...")

    // Ingest some sample documents
    let docs = [
        ("Apple announced record quarterly earnings today, with iPhone sales exceeding expectations. CEO Tim Cook praised the team's innovation.", Source::Api),
        ("The local community center will host a charity event next Saturday. Volunteers are needed for setup and registration.", Source::Email),
        ("Scientists at MIT have developed a new battery technology that could triple electric vehicle range while reducing costs by 40%.", Source::Upload)
    ]

    for (content, source) in docs {
        let id = ask(api, IngestDocument(source, content))
        log::info("Ingested document: {id}")
    }

    // Wait for processing
    sleep(Duration::seconds(3))

    // Search
    log::info("Searching for 'technology'...")
    let results = ask(api, Search("technology", limit: 5))

    for result in results {
        print("---")
        print("Score: {result.score:.2}")
        print("Summary: {result.document.summary}")
        print("Sentiment: {result.document.sentiment}")
        print("Topics: {result.document.topics.join(\", \")}")
    }

    // Get stats
    let stats = ask(api, GetSystemStats)
    print("---")
    print("System Statistics:")
    print("  Documents ingested: {stats.ingested}")
    print("  Documents processed: {stats.processed}")
    print("  Entities extracted: {stats.entities_extracted}")

    // Keep running
    log::info("System running. Press Ctrl+C to stop.")
    wait_forever()
}
```

---

## Step 8: Configuration

```toml
# simplex.toml
[package]
name = "document-processor"
version = "0.1.0"

[dependencies]
# Add dependencies here

[ai]
default_model = "default"
embedding_model = "bge-large"

[deployment]
cloud = "aws"
region = "us-east-1"
instance_type = "t4g.micro"
spot_enabled = true
min_nodes = 3
max_nodes = 10

[deployment.ai]
gpu_instance = "g4dn.xlarge"
gpu_count = 2

[logging]
level = "info"
format = "json"
```

---

## Running the Project

```bash
# Build
simplex build

# Run locally
simplex run

# Run tests
simplex test

# Deploy to cloud
simplex deploy

# Monitor
simplex swarm status
simplex logs --follow
simplex costs --watch
```

---

## Developer Tools (v0.12.0)

Simplex 0.12.0 introduces a comprehensive suite of developer tools to help you write better code, catch bugs early, and optimize performance.

### Code Formatter (sxfmt)

Keep your code consistent and readable with the built-in formatter:

```bash
# Format a single file
sxfmt src/main.sx

# Format entire project
sxfmt .

# Check formatting without modifying (useful for CI)
sxfmt --check .

# Format with specific style options
sxfmt --indent-width 4 --max-line-length 100 src/
```

The formatter follows the official Simplex style guide by default. Add a `.sxfmt.toml` file to customize:

```toml
# .sxfmt.toml
indent_width = 4
max_line_length = 100
trailing_comma = true
```

### Static Linter (sxlint)

Catch potential bugs and code smells before they become problems:

```bash
# Lint a single file
sxlint src/main.sx

# Lint entire project
sxlint .

# Lint with specific rules
sxlint --rules unused-vars,dead-code .

# Show all available lint rules
sxlint --list-rules
```

Common lint rules include:
- `unused-vars` - Detect unused variables
- `dead-code` - Find unreachable code
- `actor-blocking` - Warn about blocking operations in actors
- `unsafe-unwrap` - Flag `.unwrap()` calls without error handling

Configure rules in `simplex.toml`:

```toml
[lint]
rules = ["all"]
deny = ["unsafe-unwrap", "actor-blocking"]
warn = ["unused-vars"]
allow = ["dead-code"]
```

### Benchmarking (sxc bench)

Measure and track performance of your code:

```bash
# Run all benchmarks
sxc bench

# Run specific benchmark
sxc bench --filter "processor"

# Compare against baseline
sxc bench --baseline main

# Output results as JSON
sxc bench --output json > results.json
```

Create benchmark files in `benches/`:

```simplex
// benches/processor_bench.sx
use benchmark::*

bench ProcessorThroughput {
    setup {
        let docs = generate_test_documents(1000)
    }

    run {
        for doc in docs {
            processor.process(doc)
        }
    }
}

bench SingleDocumentLatency {
    iterations: 1000,

    run {
        processor.process(sample_document())
    }
}
```

### Code Coverage (sxc test --coverage)

Understand how well your tests cover your code:

```bash
# Run tests with coverage
sxc test --coverage

# Generate HTML coverage report
sxc test --coverage --coverage-report html

# Set minimum coverage threshold (fails if below)
sxc test --coverage --min-coverage 80

# Coverage for specific modules
sxc test --coverage --coverage-include "src/actors/*"
```

Coverage output shows:
- Line coverage percentage
- Branch coverage
- Uncovered lines highlighted
- Coverage trends over time

Example output:
```
Coverage Report
---------------
src/actors/processor.sx    87.3%  (142/163 lines)
src/actors/storage.sx      92.1%  (105/114 lines)
src/types.sx              100.0%  (48/48 lines)
---------------
Total:                     91.2%  (295/325 lines)
```

### Error Explanations (sxc explain)

Get detailed explanations for compiler errors:

```bash
# Explain a specific error code
sxc explain E0423

# Explain with examples
sxc explain E0423 --examples

# List all error codes
sxc explain --list
```

Example usage:
```bash
$ sxc explain E0423

Error E0423: Cannot send message to actor from non-async context

This error occurs when you try to use `send` or `ask` to communicate
with an actor outside of an async function or actor receive block.

Problem:
  fn main() {
      let actor = spawn MyActor
      send(actor, DoSomething)  // E0423: not in async context
  }

Solution:
  Use `async fn` or call from within an actor:

  async fn main() {
      let actor = spawn MyActor
      send(actor, DoSomething)  // OK: async context
  }

See also: Chapter 7 - Introduction to Actors
```

### Integrating Tools in Your Workflow

Add these to your CI pipeline:

```yaml
# .github/workflows/ci.yml
jobs:
  check:
    steps:
      - name: Format check
        run: sxfmt --check .

      - name: Lint
        run: sxlint .

      - name: Test with coverage
        run: sxc test --coverage --min-coverage 80

      - name: Benchmarks
        run: sxc bench --baseline main
```

Or use the combined check command:

```bash
# Run format check, lint, and tests together
sxc check

# Run everything including benchmarks
sxc check --all
```

---

## What We Built

```
                    ┌─────────────────────────────────────────┐
                    │              Application                │
                    │            (Supervisor)                 │
                    └───────────────────┬─────────────────────┘
                                        │
            ┌───────────────────────────┼───────────────────────────┐
            │                           │                           │
            ▼                           ▼                           ▼
    ┌───────────────┐           ┌───────────────┐           ┌───────────────┐
    │ StorageLayer  │           │ProcessingLayer│           │IngestionLayer │
    │ (Supervisor)  │           │ (Supervisor)  │           │ (Supervisor)  │
    └───────┬───────┘           └───────┬───────┘           └───────┬───────┘
            │                           │                           │
            ▼                           ▼                           ▼
    ┌───────────────┐           ┌───────────────┐           ┌───────────────┐
    │DocumentStorage│◄──────────│ ProcessorPool │◄──────────│DocumentIngeste│
    │               │           │               │           │               │
    │ • Store docs  │           │ • 4 workers   │           │ • Queue docs  │
    │ • Search      │           │ • AI analysis │           │ • Route       │
    │ • Checkpoint  │           │ • Parallel    │           │ • Stats       │
    └───────────────┘           └───────────────┘           └───────────────┘
```

---

## Key Patterns Used

| Pattern | Where | Why |
|---------|-------|-----|
| Actor isolation | All components | No shared state, no race conditions |
| Message passing | Between actors | Decoupled, async communication |
| Supervision | App, layers | Automatic crash recovery |
| Checkpointing | Storage, ingester | State survives restarts |
| Parallel AI | Processor | Faster analysis |
| Worker pool | ProcessorPool | Load distribution |
| Type safety | Types module | Compile-time correctness |

---

## Congratulations!

You've completed the Simplex tutorial. You now know how to:

- Write type-safe Simplex code
- Define and use custom types
- Create actors with state and message handlers
- Build supervision trees for fault tolerance
- Use checkpointing for state persistence
- Integrate AI capabilities
- Structure a real application

---

## Next Steps

1. **Build Cognitive Hives**: Continue to [Chapter 12: Cognitive Hives](12-cognitive-hives.md) to learn about orchestrating swarms of small language models

2. **Read the specification**: Deep-dive into [Language Syntax](../spec/04-language-syntax.md) and [AI Integration](../spec/07-ai-integration.md)

3. **Deploy to production**: Learn about [Cost Optimization](../spec/08-cost-optimization.md)

4. **Explore examples**: Check out the [Document Pipeline](../examples/document-pipeline.md) for more patterns

5. **Build something**: The best way to learn is to build your own project!

---

*Next: [Chapter 12: Cognitive Hives →](12-cognitive-hives.md)*
