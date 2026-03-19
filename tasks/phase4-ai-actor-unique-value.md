# Phase 4: The Anima - Simplex's Cognitive Soul

**Priority**: CRITICAL - Simplex's unique identity
**Status**: Not Started
**Depends On**: Phases 1-3

## Overview

Every Simplex solution has an **anima** - its soul. The anima is not a library or an integration.
It IS the cognitive heart of the application. A local SLM that embodies:

- **Memory** - what it knows (episodic, semantic, procedural)
- **Beliefs** - what it holds true (with confidence and evidence)
- **Desires** - what it wants (goals)
- **Intentions** - what it's doing about it (plans)
- **Identity** - who it is (purpose, values)

The anima is the beating heart, mind, and soul of every Simplex AI application.
All other constructs (actors, specialists, hives) integrate WITH the anima.

```
┌─────────────────────────────────────────────────────────────────┐
│                         SIMPLEX SOLUTION                         │
│                                                                   │
│    ┌─────────────────────────────────────────────────────────┐   │
│    │                        ANIMA                             │   │
│    │                   (Cognitive Soul)                       │   │
│    │                                                          │   │
│    │   ┌──────────┐  ┌──────────┐  ┌──────────┐             │   │
│    │   │  Memory  │  │ Beliefs  │  │ Desires  │             │   │
│    │   └──────────┘  └──────────┘  └──────────┘             │   │
│    │                                                          │   │
│    │   ┌──────────┐  ┌──────────┐  ┌──────────┐             │   │
│    │   │Intentions│  │ Identity │  │   SLM    │             │   │
│    │   └──────────┘  └──────────┘  └──────────┘             │   │
│    │                      ▲                                   │   │
│    └──────────────────────┼───────────────────────────────────┘   │
│                           │                                       │
│         ┌─────────────────┼─────────────────┐                    │
│         ▼                 ▼                 ▼                    │
│    ┌─────────┐      ┌───────────┐     ┌─────────┐               │
│    │ Actors  │      │Specialists│     │  Hives  │               │
│    └─────────┘      └───────────┘     └─────────┘               │
│                                                                   │
└───────────────────────────────────────────────────────────────────┘
```

---

## 1. The Anima Construct

**Priority**: CRITICAL
**Status**: New language keyword

The `anima` is a first-class language construct, like `actor`, `specialist`, and `hive`.
It is the cognitive soul - the beating heart, mind, and memory of every Simplex AI solution.

### 1.1 Anima Definition

```simplex
anima ProjectAssistant {
    /// Identity - who this anima is
    identity {
        purpose: "Help developers build better Simplex code",
        values: ["clarity", "correctness", "simplicity"],
        personality: "patient, thorough, encouraging",
    }

    /// Memory - what this anima knows
    memory {
        episodic: EpisodicStore::new(),    // Experiences, conversations
        semantic: SemanticStore::new(),     // Facts, knowledge
        procedural: ProceduralStore::new(), // Skills, patterns
        working: WorkingMemory::new(10),    // Active context
    }

    /// Beliefs - what this anima holds true
    beliefs {
        revision_threshold: 0.3,
        contradiction_resolution: ConsensusWithEvidence,
    }

    /// Desires - what this anima wants (goals)
    desires: [],  // Populated at runtime

    /// Intentions - what this anima is doing about it
    intentions: [],  // Active plans

    /// The SLM that powers this anima
    slm: CognitiveCore::default(),
}
```

### 1.2 Core Anima Operations

```simplex
// Create and initialize anima
let anima = ProjectAssistant::new();

// Memory operations
anima.remember(experience);           // Store experience
anima.learn(fact);                    // Learn knowledge
anima.store_skill(procedure);         // Store skill

// Belief operations
anima.believe(belief, confidence);    // Form belief
anima.revise(belief, evidence);       // Update belief
anima.question(belief);               // Challenge belief

// Goal-directed operations
anima.desire(goal);                   // Set a goal
anima.intend(plan);                   // Form intention
anima.act();                          // Execute intentions

// Recall - goal-directed, not just search
let relevant = anima.recall_for(goal, context)?;

// Think - invoke the SLM for reasoning
let thought = anima.think(question)?;
```

### 1.3 Anima Persistence

```simplex
// Anima persists across sessions - it has continuity
anima.save("project_assistant.anima")?;

// Load existing anima - memories, beliefs, identity intact
let anima = ProjectAssistant::load("project_assistant.anima")?;

// Or auto-persistence
anima ProjectAssistant {
    persistence {
        path: "~/.simplex/anima/project_assistant.anima",
        auto_save: true,
        save_interval: 5m,
    }
}
```

### 1.4 Anima Integration with Actors

```simplex
// Actors can have an anima - giving them a soul
actor CodeReviewer {
    anima: ProjectAssistant,  // This actor has a soul
    tools: [read_file, write_file, run_tests],

    receive {
        Review(code: String) => {
            // The anima remembers past reviews
            let past = self.anima.recall_for(
                goal: "review similar code",
                context: code,
            )?;

            // The anima thinks about the code
            let analysis = self.anima.think(f"Review this code: {code}")?;

            // The anima remembers this review
            self.anima.remember(Experience {
                content: f"Reviewed code: {analysis}",
                importance: 0.7,
            });

            analysis
        }
    }
}
```

### 1.5 Shared Anima (Hive Mind)

```simplex
// Multiple actors can share an anima
let shared_soul = ProjectAssistant::new();

actor Researcher { anima: shared_soul.view(read_write: true) }
actor Analyst    { anima: shared_soul.view(read_write: true) }
actor Writer     { anima: shared_soul.view(read_only: true)  }

// All actors share memories, beliefs, goals
// Changes propagate to all participants
```

### 1.6 Anima SLM Configuration

```simplex
anima ProjectAssistant {
    slm {
        model: "simplex-anima-7b",  // Ships with Simplex
        quantization: Q4_K_M,        // Runs on consumer hardware
        context_window: 8192,

        // GPU acceleration
        device: Metal,  // or CUDA, Vulkan, CPU

        // Code-aware capabilities
        code_understanding: true,
        simplex_native: true,  // Understands Simplex syntax
    }
}
```

---

## 2. Cognitive Memory Architecture

**Package**: Built-in language construct
**Status**: BDI foundations exist in runtime, need full implementation
**Priority**: CRITICAL

Simplex memory is NOT a vector database with semantic search. It's a cognitive system with:
- Multiple memory types (episodic, semantic, procedural, working)
- Belief management with confidence and revision
- SLM-powered consolidation and forgetting
- Goal-directed recall

### Subtasks

- [ ] 2.1 Memory actor construct
  ```simplex
  memory CognitiveMemory {
      episodic: EpisodicStore,     // Experiences, events, conversations
      semantic: SemanticStore,     // Facts, knowledge, learned concepts
      procedural: ProceduralStore, // Skills, patterns, how-to
      working: WorkingMemory,      // Active context, short-term

      specialist: memory_slm,      // SLM for memory operations

      config {
          consolidation_interval: 1h,
          max_working_memory: 10,
          belief_revision_threshold: 0.3,
      }
  }
  ```

- [ ] 2.2 Memory operations
  ```simplex
  // Remember an experience (episodic)
  memory.remember(Experience {
      content: "User asked about error handling",
      context: current_context,
      importance: 0.8,
      timestamp: now(),
  });

  // Learn a fact (semantic)
  memory.learn(Fact {
      content: "Simplex uses actors for concurrency",
      source: "documentation",
      confidence: 0.95,
  });

  // Store a procedure (procedural)
  memory.store_procedure(Procedure {
      name: "handle_api_error",
      steps: [...],
      success_rate: 0.9,
  });
  ```

- [ ] 2.3 Goal-directed recall
  ```simplex
  // Recall is NOT just semantic search - it's goal-directed
  let relevant = memory.recall(
      goal: "Help user fix their bug",
      context: current_conversation,
      memory_types: [Episodic, Semantic],
  )?;

  // Returns memories ranked by relevance TO THE GOAL
  for mem in relevant {
      println(f"Memory: {mem.content}");
      println(f"Relevance to goal: {mem.goal_relevance}");
      println(f"Recency: {mem.recency_score}");
  }
  ```

- [ ] 2.4 Belief system
  ```simplex
  // Beliefs are not just stored - they're managed
  memory.believe(Belief {
      content: "The user prefers verbose output",
      confidence: 0.7,
      evidence: ["user said 'show me details'", "user enabled debug mode"],
  });

  // Beliefs can be revised with new evidence
  memory.revise_belief(
      belief: "user prefers verbose output",
      new_evidence: "user said 'too much output'",
  );  // Confidence decreases or belief is revised

  // Contradictions are detected and resolved
  memory.believe(Belief {
      content: "The user prefers terse output",
      confidence: 0.8,
  });  // System detects contradiction, uses SLM to resolve
  ```

- [ ] 2.5 SLM-powered consolidation
  ```simplex
  // Memory consolidation - SLM summarizes and prunes
  memory.consolidate();  // Runs on interval or manually

  // What consolidation does:
  // 1. Summarizes similar episodic memories
  // 2. Extracts semantic facts from experiences
  // 3. Updates belief confidences based on patterns
  // 4. Forgets low-importance, old memories
  // 5. Strengthens frequently-accessed memories
  ```

- [ ] 2.6 Working memory management
  ```simplex
  // Working memory is the active context
  memory.working.push(current_task);
  memory.working.push(user_message);

  // Automatic overflow to episodic
  if memory.working.len() > memory.config.max_working_memory {
      let oldest = memory.working.pop_oldest();
      memory.remember(oldest.to_experience());
  }

  // Get current context for specialist
  let context = memory.working.to_context();
  ```

- [ ] 2.7 Memory persistence
  ```simplex
  // Memory persists across sessions
  let memory = CognitiveMemory::load("assistant.memory")?;

  // ... actor runs ...

  memory.save("assistant.memory")?;

  // Or auto-save on consolidation
  memory CognitiveMemory {
      config {
          auto_save: true,
          save_path: "assistant.memory",
      }
  }
  ```

- [ ] 2.8 Memory sharing between actors
  ```simplex
  // Shared memory for hive/team of actors
  let shared = SharedMemory::new();

  actor Researcher {
      memory: shared.view(read_write: [Semantic], read_only: [Episodic]),
  }

  actor Writer {
      memory: shared.view(read_only: [Semantic, Episodic]),
  }
  ```

---

## 3. Tool/Function Calling Framework

**Package**: `simplex-tools`
**Status**: Not implemented
**Priority**: HIGH

### Subtasks

- [ ] 3.1 Tool definition
  ```simplex
  #[tool]
  /// Search the web for information
  fn web_search(
      /// The search query
      query: String,
      /// Maximum number of results
      max_results: i64,
  ) -> Vec<SearchResult> {
      // Implementation
  }

  #[tool]
  /// Execute a shell command
  fn run_command(command: String) -> CommandResult {
      // Implementation
  }
  ```

- [ ] 3.2 Tool registry
  ```simplex
  let tools = ToolRegistry::new()
      .register(web_search)
      .register(run_command)
      .register(read_file)
      .register(write_file);
  ```

- [ ] 3.3 Automatic schema generation
  - [ ] Generate JSON schema from function signature
  - [ ] Include documentation in schema
  - [ ] Type mapping to JSON types

- [ ] 3.4 Tool execution loop
  ```simplex
  let runner = ToolRunner::new(specialist)
      .tools(tools)
      .max_iterations(10);

  let result = runner.run("Find the weather in Tokyo and save it to weather.txt")?;
  ```

- [ ] 3.5 Tool result handling
  - [ ] Success/failure propagation
  - [ ] Output formatting for LLM
  - [ ] Error recovery strategies

- [ ] 3.6 Built-in tools
  - [ ] `file_read`, `file_write`, `file_list`
  - [ ] `web_fetch`, `web_search`
  - [ ] `shell_execute`
  - [ ] `http_request`
  - [ ] `database_query`

---

## 4. Multi-Actor Orchestration

**Package**: `simplex-actors` (enhance existing)
**Status**: Actor/Hive primitives exist, need patterns
**Priority**: HIGH

### Subtasks

- [ ] 4.1 AI-powered actor definition
  ```simplex
  actor CodeReviewer {
      specialist: claude_specialist,
      tools: [read_file, write_file, run_tests],

      receive {
          Review(pr: PullRequest) => {
              let analysis = self.specialist.infer(pr.diff)?;
              // Use tools as needed
              ReviewResult { ... }
          }
      }
  }
  ```

- [ ] 4.2 Actor communication patterns
  ```simplex
  // Sequential pipeline
  let result = Pipeline::new()
      .step(researcher)
      .step(analyst)
      .step(writer)
      .run(input)?;

  // Parallel fan-out
  let results = Parallel::new()
      .actor(reviewer1)
      .actor(reviewer2)
      .actor(reviewer3)
      .run(code)?;

  // Consensus
  let result = Consensus::new()
      .actors([actor1, actor2, actor3])
      .strategy(ConsensusStrategy::Majority)
      .run(question)?;
  ```

- [ ] 4.3 Supervisor actor (uses existing supervisor syntax)
  ```simplex
  supervisor TaskSupervisor {
      strategy: OneForOne,
      children: [coder, tester, reviewer],

      on_failure: |child, reason| {
          log::error(f"Worker {child} failed: {reason}");
          Restart
      }
  }
  ```

- [ ] 4.4 Memory sharing
  ```simplex
  let shared_memory = SharedMemory::new();

  actor Researcher {
      memory: shared_memory,

      receive {
          Research(topic: String) => {
              let findings = self.search(topic);
              self.memory.remember("research", findings);
          }
      }
  }

  actor Writer {
      memory: shared_memory,

      receive {
          Write => {
              let research = self.memory.recall("research")?;
              // Use shared research
          }
      }
  }
  ```

- [ ] 4.5 Conversation history
  - [ ] Per-actor history
  - [ ] Shared conversation context
  - [ ] History summarization
  - [ ] Context window management

- [ ] 4.6 Actor lifecycle
  - [ ] Start/stop (spawn/stop)
  - [ ] Health checks
  - [ ] Resource limits
  - [ ] Timeout handling

---

## 5. Native Cognitive Core (SLM Runtime)

**Package**: Built-in to Simplex runtime
**Status**: Foundation exists, need full SLM integration
**Priority**: CRITICAL

The cognitive core is a **local SLM** that ships with Simplex. It's not a wrapper around
external services - it IS the intelligence layer. No vector databases, no embedding APIs,
no external dependencies.

### Subtasks

- [ ] 5.1 Native SLM integration
  ```simplex
  // The SLM is built into the runtime
  // It understands Simplex code, types, actors, memory

  // Every Simplex application has access to the cognitive core
  let core = CognitiveCore::default();  // Local SLM

  // Or configure a specific model
  let core = CognitiveCore::new(
      model: "simplex-slm-7b",  // Ships with Simplex
      context_window: 8192,
      quantization: Q4_K_M,     // Runs on consumer hardware
  );
  ```

- [ ] 5.2 SLM-native memory (no vectors)
  ```simplex
  // Memory is managed BY the SLM, not as vectors
  // The SLM understands context, relationships, importance natively

  core.remember("User prefers functional style");
  core.remember("Project uses actor-based architecture");

  // Recall is semantic understanding, not vector similarity
  let relevant = core.recall_for(goal: "Help refactor this code");
  ```

- [ ] 5.3 Code-aware cognition
  ```simplex
  // The SLM understands Simplex code structure
  core.understand_codebase("./src");

  // It can reason about the code, not just search it
  let insights = core.analyze(
      code: some_function,
      question: "What are the failure modes?",
  );
  ```

- [ ] 5.4 Runtime integration
  ```simplex
  // The SLM can observe and learn from runtime behavior
  actor MyActor {
      core: CognitiveCore,

      receive {
          Request(data) => {
              // SLM sees patterns in requests over time
              self.core.observe(data);

              // And can predict/optimize based on learned patterns
              let strategy = self.core.suggest_strategy(data);
          }
      }
  }
  ```

- [ ] 5.5 Distributed cognition
  ```simplex
  // SLMs can share knowledge across a hive
  hive DistributedMind {
      specialists: [analyst, coder, reviewer],

      // Shared cognitive core - knowledge flows between specialists
      shared_core: CognitiveCore::shared(),
  }
  ```

- [ ] 5.6 Model formats and deployment
  - [ ] GGUF format support (llama.cpp compatible)
  - [ ] Quantization options (Q4, Q5, Q8, F16)
  - [ ] GPU acceleration (Metal, CUDA, Vulkan)
  - [ ] CPU fallback for portability
  - [ ] Model bundling with Simplex applications

---

## 6. Specialist Enhancements

**Package**: Built-in, enhance existing
**Status**: Basic exists
**Priority**: HIGH

### Subtasks

- [ ] 7.1 Multi-provider support
  ```simplex
  specialist Coder {
      provider: Anthropic,
      model: "claude-3-opus",
      // or
      provider: OpenAI,
      model: "gpt-4-turbo",
      // or
      provider: Ollama,
      model: "codellama:34b",
  }
  ```

- [ ] 7.2 Streaming responses
  ```simplex
  let stream = specialist.infer_stream(prompt);
  for chunk in stream {
      print(chunk.text);
  }
  ```

- [ ] 7.3 Structured output
  ```simplex
  #[derive(JsonSchema)]
  struct CodeReview {
      issues: Vec<Issue>,
      suggestions: Vec<String>,
      score: i64,
  }

  let review: CodeReview = specialist.infer_structured(prompt)?;
  ```

- [ ] 7.4 Vision support
  ```simplex
  let analysis = specialist.infer(prompt! {
      user: "What's in this image?",
      images: [image_bytes],
  })?;
  ```

- [ ] 7.5 Token counting and limits
  ```simplex
  let tokens = specialist.count_tokens(prompt);
  if tokens > specialist.context_limit() {
      prompt = prompt.truncate(specialist.context_limit() - 1000);
  }
  ```

- [ ] 7.6 Cost tracking
  ```simplex
  let (response, usage) = specialist.infer_with_usage(prompt)?;
  println(f"Input tokens: {usage.input_tokens}");
  println(f"Output tokens: {usage.output_tokens}");
  println(f"Cost: ${usage.cost}");
  ```

- [ ] 7.7 Retry and fallback
  ```simplex
  specialist Robust {
      primary: claude_specialist,
      fallback: gpt_specialist,
      retries: 3,
      timeout: 30s,
  }
  ```

---

## 7. Hive Enhancements

**Package**: Built-in, enhance existing
**Status**: Basic routing exists
**Priority**: MEDIUM

### Subtasks

- [ ] 8.1 Dynamic specialist registration
  ```simplex
  let hive = Hive::new()
      .add_specialist("coder", coder_spec)
      .add_specialist("reviewer", reviewer_spec)
      .add_specialist("writer", writer_spec);

  // Add at runtime
  hive.register("analyst", analyst_spec);
  ```

- [ ] 8.2 Semantic routing
  ```simplex
  hive Router {
      specialists: [coder, reviewer, writer, analyst],

      // Automatic routing based on query understanding
      routing: Semantic {
          embedder: openai_embedder,
          examples: [
              ("Write a function to...", "coder"),
              ("Review this code...", "reviewer"),
              ("Explain how...", "writer"),
          ],
      },
  }
  ```

- [ ] 8.3 Load balancing
  ```simplex
  hive Pool {
      specialists: [worker1, worker2, worker3],

      routing: LoadBalanced {
          strategy: LeastBusy,
          health_check: 30s,
      },
  }
  ```

- [ ] 8.4 Specialist composition
  ```simplex
  hive Pipeline {
      stages: [
          Stage::new(researcher).output("research"),
          Stage::new(analyst).input("research").output("analysis"),
          Stage::new(writer).input("analysis"),
      ],
  }

  let result = pipeline.run(query)?;
  ```

- [ ] 8.5 Consensus and voting
  ```simplex
  hive Panel {
      specialists: [expert1, expert2, expert3],

      aggregation: Consensus {
          strategy: MajorityVote,
          tie_breaker: expert1,
      },
  }
  ```

---

## 8. Actor-AI Integration

**Package**: Built-in bridge
**Status**: Separate systems, need integration
**Priority**: HIGH

### Subtasks

- [ ] 9.1 AI-powered actors
  ```simplex
  actor AIAssistant {
      specialist: claude_specialist,
      memory: ConversationMemory::new(),
      tools: [web_search, calculator],

      receive {
          UserMessage(text) => {
              // Build context from memory
              let context = self.memory.recent(10);

              // Create prompt with tools
              let response = self.specialist.infer_with_tools(
                  context + text,
                  self.tools,
              )?;

              // Store in memory
              self.memory.add(text, response);

              reply(AssistantResponse(response));
          }
      }
  }
  ```

- [ ] 9.2 Specialist as actor
  ```simplex
  // Wrap specialist in actor for concurrent access
  let specialist_actor = spawn SpecialistActor {
      specialist: claude_specialist,
      rate_limit: 10.per_minute(),
  };

  // Send inference requests
  let response = ask specialist_actor Infer(prompt);
  ```

- [ ] 9.3 Distributed AI workloads
  ```simplex
  actor AIWorkerPool {
      workers: Vec<ActorRef>,

      init {
          for i in 0..num_workers {
              self.workers.push(spawn AIWorker { id: i });
          }
      }

      receive {
          Task(work) => {
              let worker = self.select_worker();
              send worker DoWork(work);
          }
      }
  }
  ```

- [ ] 9.4 Supervision for AI failures
  ```simplex
  supervisor AITaskSupervisor {
      strategy: OneForOne,
      max_restarts: 3,
      window: 60s,

      children: [
          child(AIWorker, restart: Transient),
      ],

      on_failure: |child, reason| {
          log::error(f"AI worker {child} failed: {reason}");
          if reason.is_rate_limit() {
              sleep(60s);
          }
          Restart
      }
  }
  ```

---

## 9. Observability

**Package**: `simplex-ai-observe`
**Status**: Not implemented
**Priority**: MEDIUM

### Subtasks

- [ ] 10.1 Request logging
  ```simplex
  let specialist = specialist.with_logging(Logger::new("ai.log"));
  // Logs all prompts and responses
  ```

- [ ] 10.2 Metrics collection
  ```simplex
  let metrics = AIMetrics::new();
  let specialist = specialist.with_metrics(metrics);

  // Later
  println(f"Total requests: {metrics.request_count()}");
  println(f"Avg latency: {metrics.avg_latency()}ms");
  println(f"Total cost: ${metrics.total_cost()}");
  println(f"Token usage: {metrics.total_tokens()}");
  ```

- [ ] 10.3 Tracing
  ```simplex
  #[traced]
  fn process_query(query: String) -> String {
      let embedding = embedder.embed(query);  // Traced
      let docs = store.search(embedding);     // Traced
      let response = specialist.infer(prompt); // Traced
      response
  }
  ```

- [ ] 10.4 Evaluation framework
  ```simplex
  let eval = Evaluation::new()
      .dataset(test_cases)
      .metric(Accuracy)
      .metric(F1Score)
      .metric(AnswerRelevance);

  let results = eval.run(my_pipeline)?;
  results.report();
  ```

- [ ] 10.5 A/B testing
  ```simplex
  let experiment = Experiment::new("prompt_v2")
      .control(prompt_v1)
      .variant(prompt_v2)
      .split(50);  // 50/50 split

  let prompt = experiment.select();
  let response = specialist.infer(prompt)?;
  experiment.record(response, metrics);
  ```

---

## Completion Criteria

Phase 4 is complete when:
- [ ] `anima` keyword is implemented in parser/lexer/codegen
- [ ] Cognitive memory (episodic, semantic, procedural, working) works
- [ ] Belief system with revision and contradiction detection works
- [ ] Native SLM (simplex-anima) ships with runtime
- [ ] Tool calling works with automatic schema generation
- [ ] Multi-actor orchestration patterns are documented
- [ ] Actor-anima integration is seamless
- [ ] Example: Build a complete AI assistant with persistent anima

---

## Example Application: AI Code Assistant with Anima

```simplex
// Demonstrate Phase 4 capabilities - the anima-centric approach

/// The soul of our code assistant
anima DevSoul {
    identity {
        purpose: "Be the best pair programmer a developer could have",
        values: ["correctness", "clarity", "teaching", "patience"],
        personality: "friendly, thorough, encouraging",
    }

    memory {
        episodic: EpisodicStore::new(),     // Past conversations, reviews
        semantic: SemanticStore::new(),      // Learned patterns, preferences
        procedural: ProceduralStore::new(),  // Code patterns, solutions
        working: WorkingMemory::new(20),     // Current context
    }

    beliefs {
        revision_threshold: 0.3,
        contradiction_resolution: EvidenceBased,
    }

    slm {
        model: "simplex-anima-7b",
        quantization: Q4_K_M,
        device: Metal,
        simplex_native: true,
    }

    persistence {
        path: "~/.simplex/anima/dev_soul.anima",
        auto_save: true,
        save_interval: 5m,
    }
}

/// Code reviewer actor - powered by the shared soul
actor CodeReviewer {
    anima: DevSoul,
    tools: [read_file, run_tests, analyze_coverage],

    receive {
        Review(code: String) => {
            // Recall similar code reviews from memory
            let past_reviews = self.anima.recall_for(
                goal: "Find similar code patterns I've reviewed",
                context: code,
            )?;

            // Think about the code with context
            let analysis = self.anima.think(f"""
                Review this code for bugs, style, and improvements.
                Consider these past similar reviews: {past_reviews}
                Code: {code}
            """)?;

            // Learn from this review
            self.anima.remember(Experience {
                content: f"Reviewed: {code.summary()}, Found: {analysis.summary()}",
                importance: 0.7,
            });

            // Update beliefs about coding patterns
            if analysis.found_common_bug() {
                self.anima.believe(
                    Belief::new(f"Pattern {analysis.pattern} often has bugs"),
                    confidence: 0.6,
                );
            }

            reply(ReviewResult(analysis));
        }
    }
}

/// Code explainer actor - shares the same soul
actor CodeExplainer {
    anima: DevSoul,
    tools: [read_file, search_docs],

    receive {
        Explain(code: String) => {
            // The anima knows user's skill level from past conversations
            let user_level = self.anima.recall_for(
                goal: "What is this user's skill level?",
                context: "user expertise",
            )?;

            let explanation = self.anima.think(f"""
                Explain this code at a {user_level} level.
                Be {self.anima.identity.personality}.
                Code: {code}
            """)?;

            reply(Explanation(explanation));
        }
    }
}

/// Hive that orchestrates the soul-powered actors
hive CodeAssistant {
    specialists: [
        spawn CodeReviewer { anima: DevSoul::shared() },
        spawn CodeExplainer { anima: DevSoul::shared() },
    ],

    routing: IntentBased {
        patterns: [
            ("review", "check", "bugs")   => "CodeReviewer",
            ("explain", "what", "how")    => "CodeExplainer",
        ],
    },
}

fn main() {
    // Load or create the soul - memories persist across sessions
    let soul = DevSoul::load_or_create()?;

    // Create the assistant hive
    let assistant = spawn CodeAssistant { anima: soul };

    println("DevSoul Code Assistant initialized.");
    println(f"I remember {soul.memory.episodic.len()} past conversations.");

    // Interactive loop
    loop {
        let query = read_line("You: ")?;

        // The soul's working memory tracks the conversation
        soul.working.push(UserMessage(query));

        // Route to appropriate specialist (all share the soul)
        let response = ask assistant Route(query);

        // Soul learns from every interaction
        soul.working.push(AssistantResponse(response));

        println(f"Assistant: {response}");
    }
}
```

---

## Dependencies

- Phase 1: Core (HashMap, JSON, iterators)
- Phase 2: Package ecosystem
- Phase 3: HTTP, crypto, logging

## Impact

This phase establishes Simplex as **the** language for AI-native applications, combining:
- Erlang-style actors for reliability
- First-class AI primitives
- Type-safe LLM interactions
- Production-ready orchestration patterns
