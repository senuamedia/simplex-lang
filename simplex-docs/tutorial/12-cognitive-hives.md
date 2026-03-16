# Chapter 12: Building Cognitive Hives

In Chapter 10, we learned basic AI integration with `ai::complete`, `ai::classify`, and `ai::extract`. Those primitives call external AI services. In this chapter, we go deeper: building **cognitive hives**—coordinated swarms of small language models (SLMs) that run locally within your Simplex swarm.

---

## Why Hives?

| Approach | Latency | Cost | Control | Privacy |
|----------|---------|------|---------|---------|
| External LLM API | 500-3000ms | High | Low | Data leaves |
| Single local SLM | 50-200ms | Low | High | Data stays |
| **Cognitive Hive** | 50-200ms | Low | High | Data stays |

A hive gives you the benefits of local inference plus the power of specialized models working together.

---

## Your First Specialist

A **specialist** is an actor that wraps a small language model:

```simplex
specialist Summarizer {
    model: "mistral-7b-instruct",
    domain: "text summarization",

    receive Summarize(text: String) -> String {
        infer("Summarize the following text in 2-3 sentences:\n\n{text}")
    }

    receive SummarizeBullets(text: String, count: i64) -> String {
        infer("Summarize in exactly {count} bullet points:\n\n{text}")
    }
}

fn main() {
    let summarizer = spawn Summarizer

    let article = """
        The Simplex programming language introduces a novel approach to
        distributed computing by combining actor-based concurrency with
        AI-native primitives. Programs written in Simplex automatically
        distribute across swarms of lightweight virtual machines...
    """

    let summary = ask(summarizer, Summarize(article))
    print(summary)
}
```

The `infer` function is the core primitive—it sends a prompt to the specialist's model and returns the response.

---

## The `infer` Primitive

Inside a specialist, `infer` calls the underlying model:

```simplex
specialist Writer {
    model: "llama2-7b",

    receive Write(prompt: String) -> String {
        // Basic inference
        infer(prompt)
    }

    receive WriteCreative(prompt: String) -> String {
        // With temperature for creativity
        infer(prompt, temperature: 0.9, max_tokens: 500)
    }

    receive WriteStructured(prompt: String) -> Response {
        // Parse output to a type
        infer_typed<Response>(prompt)
    }
}
```

**Options:**

| Option | Type | Default | Purpose |
|--------|------|---------|---------|
| `temperature` | f64 | 0.7 | Randomness (0.0 = deterministic) |
| `max_tokens` | i64 | 256 | Maximum response length |
| `stop_sequences` | List<String> | [] | Stop generation at these strings |
| `top_p` | f64 | 1.0 | Nucleus sampling threshold |

---

## Creating a Hive

A **hive** coordinates multiple specialists:

```simplex
specialist Summarizer {
    model: "mistral-7b-instruct",
    domain: "summarization",

    receive Summarize(text: String) -> String {
        infer("Summarize: {text}")
    }
}

specialist EntityExtractor {
    model: "ner-7b",
    domain: "entity extraction",

    receive Extract(text: String) -> List<Entity> {
        let raw = infer("Extract named entities: {text}")
        parse_entities(raw)
    }
}

specialist SentimentAnalyzer {
    model: "sentiment-3b",
    domain: "sentiment analysis",

    receive Analyze(text: String) -> Sentiment {
        let raw = infer("Classify sentiment (positive/negative/neutral): {text}")
        parse_sentiment(raw)
    }
}

// The hive coordinates the specialists
hive DocumentAnalyzer {
    specialists: [Summarizer, EntityExtractor, SentimentAnalyzer],
    router: SemanticRouter
}

fn main() {
    let hive = spawn DocumentAnalyzer

    let doc = "Apple Inc. reported record quarterly earnings..."

    // Access specific specialists
    let summary = ask(hive.Summarizer, Summarize(doc))
    let entities = ask(hive.EntityExtractor, Extract(doc))
    let sentiment = ask(hive.SentimentAnalyzer, Analyze(doc))

    print("Summary: {summary}")
    print("Entities: {entities}")
    print("Sentiment: {sentiment}")
}
```

---

## Routing Tasks

Instead of manually picking specialists, let the hive route:

### Semantic Router

Routes based on task similarity to specialist domains:

```simplex
hive SmartHive {
    specialists: [Summarizer, Translator, CodeReviewer, Explainer],

    router: SemanticRouter(
        embedding_model: "all-minilm-l6",
        threshold: 0.6
    )
}

fn main() {
    let hive = spawn SmartHive

    // Hive automatically routes to Summarizer
    let r1 = ask(hive, Route("Give me the key points of this article..."))

    // Hive automatically routes to Translator
    let r2 = ask(hive, Route("Translate this to Spanish: Hello world"))

    // Hive automatically routes to CodeReviewer
    let r3 = ask(hive, Route("Review this function for bugs: fn add(a, b)..."))
}
```

### Rule Router

Explicit pattern matching:

```simplex
hive RuleBasedHive {
    specialists: [Summarizer, Translator, Calculator],

    router: RuleRouter(
        rules: [
            (r"summar|brief|tldr|key points", Summarizer),
            (r"translat|convert to|in spanish|in french", Translator),
            (r"calculat|compute|what is \d", Calculator)
        ],
        fallback: Summarizer
    )
}
```

---

## Parallel Processing

Run multiple specialists simultaneously:

```simplex
fn comprehensive_analysis(doc: String) -> FullAnalysis {
    let hive = spawn DocumentHive

    // All specialists work in parallel
    let (summary, entities, sentiment, topics, language) = await parallel(
        ask(hive.Summarizer, Summarize(doc)),
        ask(hive.EntityExtractor, Extract(doc)),
        ask(hive.SentimentAnalyzer, Analyze(doc)),
        ask(hive.TopicClassifier, Classify(doc)),
        ask(hive.LanguageDetector, Detect(doc))
    )

    FullAnalysis {
        summary,
        entities,
        sentiment,
        topics,
        language
    }
}
```

---

## Ensembles and Voting

For high-stakes decisions, use multiple specialists:

```simplex
fn classify_with_confidence(text: String) -> ClassificationResult {
    let hive = spawn ClassifierHive

    // Get opinions from three different models
    let opinions = await parallel(
        ask(hive.ClassifierA, Classify(text)),
        ask(hive.ClassifierB, Classify(text)),
        ask(hive.ClassifierC, Classify(text))
    )

    // Count votes
    let votes = opinions.group_by(|o| o.label)
    let winner = votes.max_by(|g| g.count())

    ClassificationResult {
        label: winner.key,
        confidence: winner.count() as f64 / 3.0,
        all_opinions: opinions
    }
}
```

### Weighted Voting

When some specialists are more authoritative:

```simplex
fn expert_analysis(case: LegalCase) -> Opinion {
    let opinions = await parallel(
        (ask(hive.SeniorLegalExpert, Analyze(case)), weight: 0.5),
        (ask(hive.JuniorAnalyst, Analyze(case)), weight: 0.3),
        (ask(hive.ComplianceCheck, Review(case)), weight: 0.2)
    )

    weighted_consensus(opinions)
}
```

---

## Chaining Specialists

Sequential pipelines where output flows through:

```simplex
fn research_pipeline(query: String) -> Report {
    let hive = spawn ResearchHive

    // Step 1: Parse the query
    let parsed = ask(hive.QueryParser, Parse(query))

    // Step 2: Search for information
    let sources = ask(hive.Searcher, Search(parsed.keywords))

    // Step 3: Analyze each source
    let analyses = []
    for source in sources {
        analyses.push(ask(hive.Analyzer, Analyze(source)))
    }

    // Step 4: Synthesize findings
    let synthesis = ask(hive.Synthesizer, Combine(analyses))

    // Step 5: Format as report
    ask(hive.ReportWriter, Format(synthesis))
}
```

---

## Shared Memory

Specialists can share context:

### Vector Store

```simplex
hive KnowledgeHive {
    specialists: [Researcher, Writer, FactChecker],

    // Shared vector memory
    memory: VectorStore(dimension: 384)
}

specialist Researcher {
    receive Research(topic: String) -> Findings {
        // Check what we already know
        let existing = hive.memory.search(topic, k: 5)

        let findings = infer("""
            Existing knowledge: {existing}
            Research topic: {topic}
            Provide new insights:
        """)

        // Store new findings
        hive.memory.add(findings, metadata: { topic: topic })

        findings
    }
}

specialist FactChecker {
    receive Verify(claim: String) -> VerificationResult {
        // Search shared memory for supporting evidence
        let evidence = hive.memory.search(claim, k: 10)

        infer_typed<VerificationResult>("""
            Claim: {claim}
            Available evidence: {evidence}
            Is this claim supported?
        """)
    }
}
```

### Conversation Context

```simplex
hive ChatHive {
    specialists: [Greeter, Helper, Farewell],

    // Shared conversation history
    context: ConversationBuffer(max_turns: 20)
}

specialist Helper {
    receive Help(query: String) -> String {
        // Access full conversation history
        let history = hive.context.format()

        let response = infer("""
            Conversation so far:
            {history}

            User asks: {query}
            Provide helpful response:
        """)

        // Add to shared context
        hive.context.add(Role::User, query)
        hive.context.add(Role::Assistant, response)

        response
    }
}
```

---

## Naming Your Specialists

Simplex suggests meaningful naming conventions:

### Elvish (Sindarin/Quenya)

For organic, poetic names:

| Name | Meaning | Use For |
|------|---------|---------|
| **Isto** | Knowledge | Knowledge retrieval |
| **Penna** | One who tells | Text generation |
| **Silma** | Crystal light | Clarity, summarization |
| **Curu** | Skill, craft | Code, technical work |
| **Golwen** | Wise one | Decision making |
| **Mellon** | Friend | Conversational assistant |
| **Hîr** | Master | Orchestrator, router |

```simplex
specialist Silma {
    model: "mistral-7b",
    domain: "summarization and clarity"
}

hive Mellon {
    specialists: [Silma, Curu, Isto],
    router: Hîr
}
```

### Latin

For classical, technical names:

| Name | Meaning | Use For |
|------|---------|---------|
| **Cogito** | I think | Reasoning, analysis |
| **Scribo** | I write | Text generation |
| **Lego** | I read | Document processing |
| **Sentio** | I perceive | Sentiment analysis |
| **Judex** | Judge | Decision making |
| **Index** | Pointer | Routing |
| **Vertex** | Peak | Orchestration |

```simplex
specialist Cogito {
    model: "llama2-13b",
    domain: "logical reasoning"
}

hive Vertex {
    specialists: [Cogito, Scribo, Lego],
    router: Index
}
```

---

## Dynamic Specialists

### Spawning On Demand

```simplex
hive AdaptiveHive {
    // Always-on specialists
    specialists: [GeneralAssistant],

    // Available when needed
    available: [LegalExpert, MedicalAdvisor, FinancialAnalyst],

    on_no_match(task: Task) {
        // Check if we can spawn a specialist
        for specialist_type in available {
            if specialist_type.can_handle(task) {
                let new = spawn specialist_type
                return ask(new, task)
            }
        }

        // Fallback to general
        ask(GeneralAssistant, task)
    }
}
```

### Hibernation

Save resources when specialists are idle:

```simplex
hive EfficientHive {
    idle_timeout: Duration::minutes(5),

    on_idle(specialist: Specialist, duration: Duration) {
        if duration >= idle_timeout {
            checkpoint(specialist)
            hibernate(specialist)
            log::info("{specialist.name} hibernated")
        }
    }
}
```

---

## Fault Tolerance

Hives inherit Simplex's supervision:

```simplex
hive ResilientHive {
    specialists: [Summarizer, Extractor, Classifier],

    // Restart strategy
    strategy: OneForOne,
    max_restarts: 5,
    within: Duration::minutes(1),

    on_specialist_crash(specialist: Specialist, error: Error) {
        log::warn("{specialist.name} crashed: {error}")

        // Try fallback if available
        if let Some(fallback) = find_fallback(specialist.domain) {
            route_to(fallback)
        }
    }
}
```

---

## Complete Example: Customer Support Hive

```simplex
// Specialists for customer support
specialist Greeter {
    model: "tinyllama-1b",
    domain: "greetings and small talk",

    receive Greet(message: String) -> String {
        infer("Respond warmly to: {message}")
    }
}

specialist IssueClassifier {
    model: "classifier-3b",
    domain: "issue classification",

    receive Classify(issue: String) -> IssueType {
        infer_typed<IssueType>("Classify this support issue: {issue}")
    }
}

specialist TechnicalSupport {
    model: "codellama-7b",
    domain: "technical troubleshooting",

    receive Troubleshoot(issue: String) -> Solution {
        infer_typed<Solution>("""
            Technical issue: {issue}
            Provide step-by-step troubleshooting:
        """)
    }
}

specialist BillingSupport {
    model: "mistral-7b",
    domain: "billing and accounts",

    receive HandleBilling(issue: String) -> Response {
        infer("Billing inquiry: {issue}\nProvide helpful response:")
    }
}

specialist Escalator {
    model: "llama2-7b",
    domain: "escalation decisions",

    receive ShouldEscalate(conversation: String) -> Bool {
        let result = infer("Should this be escalated to human? {conversation}")
        result.contains("yes") || result.contains("escalate")
    }
}

// The support hive
hive CustomerSupport {
    specialists: [
        Greeter,
        IssueClassifier,
        TechnicalSupport,
        BillingSupport,
        Escalator
    ],

    router: SemanticRouter,
    context: ConversationBuffer(max_turns: 50),

    strategy: OneForOne,
    max_restarts: 10
}

// Main support loop
fn handle_customer(hive: CustomerSupport, customer_id: String) {
    send(hive.Greeter, Greet("Welcome! How can I help?"))

    loop {
        let message = await receive_customer_message(customer_id)

        // Classify the issue
        let issue_type = ask(hive.IssueClassifier, Classify(message))

        // Route to appropriate specialist
        let response = match issue_type {
            IssueType::Technical => ask(hive.TechnicalSupport, Troubleshoot(message)),
            IssueType::Billing => ask(hive.BillingSupport, HandleBilling(message)),
            IssueType::General => ask(hive.Greeter, Greet(message))
        }

        send_to_customer(customer_id, response)

        // Check if we should escalate
        let history = hive.context.format()
        if ask(hive.Escalator, ShouldEscalate(history)) {
            escalate_to_human(customer_id)
            break
        }
    }
}

fn main() {
    let hive = spawn CustomerSupport

    // Handle incoming customers
    for customer in incoming_customers() {
        spawn async { handle_customer(hive, customer.id) }
    }
}
```

---

## Real-Time Learning (v0.7.0)

Starting with v0.7.0, specialists can learn and adapt during runtime:

### Learning-Enabled Specialist

```simplex
use simplex_learning::{OnlineLearner, StreamingAdam, SafeFallback};

specialist AdaptiveSummarizer {
    model: "mistral-7b-instruct",
    domain: "summarization",
    learner: OnlineLearner,

    fn init() {
        // Set up online learning with safety fallback
        self.learner = OnlineLearner::new(self.params())
            .optimizer(StreamingAdam::new(0.001))
            .fallback(SafeFallback::with_default("Unable to summarize."));
    }

    receive Summarize(text: String) -> String {
        infer("Summarize: {text}")
    }

    receive Feedback(summary: String, was_good: bool) {
        // Learn from user feedback
        if was_good {
            self.learner.learn(&PositiveFeedback(summary));
        } else {
            self.learner.learn(&NegativeFeedback(summary));
        }
    }
}

fn main() {
    let summarizer = spawn AdaptiveSummarizer

    let article = "The Simplex language..."
    let summary = ask(summarizer, Summarize(article))

    // User provides feedback
    print("Was this summary helpful? (y/n)")
    let feedback = read_line()

    // The specialist learns from feedback
    send(summarizer, Feedback(summary, feedback == "y"))
}
```

### Federated Learning Across Hive

Multiple specialists can learn together:

```simplex
use simplex_learning::distributed::{HiveLearningCoordinator, HiveLearningConfig};

hive LearningHive {
    specialists: [SecurityAnalyzer, QualityReviewer, PerformanceOptimizer],

    // All specialists share learnings
    learning: HiveLearningCoordinator::new(
        HiveLearningConfig::builder()
            .sync_interval(100)
            .aggregation(AggregationStrategy::FedAvg)
            .build()
    ),

    router: SemanticRouter
}

fn main() {
    let hive = spawn LearningHive

    // Each specialist's learnings are aggregated
    // and shared with all others
    for code_review in incoming_reviews() {
        let analysis = ask(hive, Route(code_review))
        let feedback = get_user_feedback(analysis)

        // Learning propagates across all specialists
        hive.learning.submit_feedback(feedback);
    }
}
```

### Safe Learning with Fallbacks

Ensure learning doesn't break your specialists:

```simplex
use simplex_learning::safety::{SafeLearner, SafeFallback, ConstraintManager};

specialist SafeClassifier {
    model: "classifier-3b",
    learner: SafeLearner,

    fn init() {
        let constraints = ConstraintManager::new()
            .add_hard(NoLossExplosion("loss", 100.0));

        self.learner = SafeLearner::new(
            OnlineLearner::new(self.params()),
            SafeFallback::last_good()  // Return last successful output if learning fails
        )
        .with_constraints(constraints)
        .max_failures(3);
    }

    receive Classify(text: String) -> Classification {
        match self.learner.try_forward(&text) {
            Ok(result) => result,
            Err(_) => Classification::Unknown  // Safe fallback
        }
    }
}
```

---

## High-Performance Inference (v0.12.0)

Starting with v0.12.0, Simplex provides native llama.cpp integration for optimized inference.

### Using the Inference Pipeline

```simplex
use simplex_inference::{InferencePipeline, BatchConfig, ModelLoadConfig};

// Configure the inference backend
let config = ModelLoadConfig {
    gpu_layers: 32,           // GPU offload
    context_size: 4096,
    flash_attention: true,
    ..Default::default()
};

// Create optimized pipeline
let pipeline = InferencePipeline::builder()
    .with_model("simplex-cognitive-7b.gguf", config)
    .with_batching(BatchConfig { max_size: 8, timeout_ms: 50 })
    .with_prompt_cache(1000)      // Cache tokenized prompts
    .with_response_cache(10000)   // Cache deterministic responses
    .build();

// Use in specialists
specialist FastSummarizer {
    pipeline: InferencePipeline,

    receive Summarize(text: String) -> String {
        // Uses batching, caching, and GPU acceleration automatically
        self.pipeline.infer("Summarize: " + text).await?
    }
}
```

### Batch Processing

Process multiple requests efficiently:

```simplex
fn batch_analyze(documents: Vec<String>) -> Vec<Analysis> {
    let pipeline = InferencePipeline::builder()
        .with_batching(BatchConfig { max_size: 16, timeout_ms: 100 })
        .build();

    // Requests are automatically batched for throughput
    let futures = documents.map(|doc| pipeline.infer(doc));
    await parallel(futures)
}
```

---

## Self-Learning Optimization (v0.12.0)

Specialists can now learn optimal schedules for their operation through meta-gradients.

### Self-Learning Annealing for Hyperparameters

```simplex
use simplex::optimize::anneal::{LearnableSchedule, MetaOptimizer, AnnealConfig};

specialist AdaptiveRouter {
    schedule: LearnableSchedule,

    fn init() {
        // Schedule learns cooling rate, reheat triggers, etc.
        self.schedule = LearnableSchedule::new();
    }

    fn optimize_routing(&mut self, hive: &Hive) {
        // Define objective: minimize routing latency
        let objective = |config: &RouterConfig| -> dual {
            let latency = measure_routing_latency(config, hive);
            dual::constant(latency)  // Convert to dual for gradients
        };

        // Let the schedule learn itself
        var optimizer = MetaOptimizer::new()
            .learning_rate(0.001);

        let best_config = optimizer.optimize(
            RouterConfig::default(),
            |c| c.mutate(),
            &AnnealConfig::default()
        );

        self.apply_config(best_config);
    }
}
```

### Using Dual Numbers for Gradient Tracking

```simplex
use simplex_std::dual::dual;

specialist DifferentiableClassifier {
    fn classify_with_confidence(&self, text: String) -> (Classification, dual) {
        // Forward pass with gradient tracking
        let features = self.extract_features(text);

        // All operations propagate derivatives
        let x = dual::variable(features.score);
        let confidence = x.sigmoid();  // d/dx = σ(x)(1-σ(x))

        (self.to_classification(confidence.val), confidence)
    }

    fn train_step(&mut self, text: String, label: Classification) {
        let (pred, confidence) = self.classify_with_confidence(text);

        // Gradient flows through confidence
        let loss = (confidence - dual::constant(1.0 if pred == label else 0.0)).pow(2.0);

        // Update based on gradient
        self.update_params(loss.der);
    }
}
```

---

## Summary

| Concept | Purpose |
|---------|---------|
| `specialist` | Actor wrapping an SLM |
| `hive` | Supervisor coordinating specialists |
| `infer` | Call the specialist's model |
| `infer_typed<T>` | Parse model output to type |
| `router` | Route tasks to specialists |
| `parallel` | Run specialists concurrently |
| `hive.memory` | Shared vector store |
| `hive.context` | Shared conversation buffer |
| `OnlineLearner` | Real-time learning during inference (v0.7.0) |
| `HiveLearningCoordinator` | Federated learning across hive (v0.7.0) |
| `SafeLearner` | Learning with safety constraints (v0.7.0) |
| `InferencePipeline` | High-performance batched inference (v0.12.0) |
| `LearnableSchedule` | Self-learning optimization schedules (v0.12.0) |
| `dual` | Automatic differentiation for training (v0.8.0) |

---

## Exercises

1. **Multi-Language Hive**: Create a hive with specialists for translating between English, Spanish, and French. Route translation requests automatically.

2. **Code Review Hive**: Build a hive with specialists for syntax checking, security review, and performance analysis. Combine their outputs into a comprehensive review.

3. **Research Assistant**: Create a hive that can research a topic, verify facts, and produce a well-cited report.

4. **Adaptive Hive**: Build a learning-enabled hive that improves its responses based on user feedback. Use `SafeLearner` to ensure stability.

---

*Previous: [Chapter 11: Capstone Project](11-capstone.md)* | *Next: [Chapter 13: Neural Gates](13-neural-gates.md)*

*See also: [Cognitive Hive AI Specification](../spec/09-cognitive-hive.md) | [Real-Time Learning](../spec/15-real-time-learning.md)*
