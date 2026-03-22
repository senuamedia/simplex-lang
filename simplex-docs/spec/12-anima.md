# The Anima - Persistent Agent State

**Version 0.5.0**

The `anima` is the persistent state manager for Simplex AI agents — it provides memory, belief tracking, and intention management that give agents continuity and purpose across interactions.

---

## v0.5.0: Anima in the Hive Architecture

In v0.5.0, the Anima integrates with the per-hive SLM architecture:

```
┌─────────────────────────────────────────────────┐
│                  HIVE SLM                        │
│         (One shared model per hive)              │
└──────────────────┬──────────────────────────────┘
                   │
    ┌──────────────┼──────────────┐
    │              │              │
    ▼              ▼              ▼
┌────────┐   ┌────────┐   ┌────────┐
│Analyst │   │Coder   │   │Reviewer│
│ Anima  │   │ Anima  │   │ Anima  │
│ (30%)  │   │ (30%)  │   │ (30%)  │
└────────┘   └────────┘   └────────┘
    │              │              │
    └──────────────┴──────────────┘
         HiveMnemonic (50%)
       (Shared memory layer)
```

**Key points**:
- Each specialist has its own **Anima** (personal memories, beliefs)
- All specialists share the **Hive SLM** (one model per hive)
- The **HiveMnemonic** provides a shared memory layer across specialists
- When `infer()` is called, both Anima and Mnemonic context are included

---

## Philosophy

Traditional AI systems are stateless - each request starts fresh with no memory of past interactions. The Anima changes this fundamentally:

| Traditional AI | Anima-Powered AI |
|----------------|------------------|
| Stateless | Remembers experiences |
| No learning | Learns from interactions |
| No beliefs | Forms and revises beliefs |
| No goals | Has desires and intentions |
| Generic | Has personality and purpose |

The Anima draws inspiration from:
- **Cognitive Psychology**: Episodic and semantic memory systems
- **BDI Architecture**: Beliefs, Desires, Intentions model from AI research
- **Philosophy of Mind**: The concept of persistent identity and agency (historical inspiration for the naming)

---

## Core Constructs

### The `anima` Keyword

```simplex
anima AssistantSoul {
    identity: {
        purpose: "Help users solve programming problems",
        personality: "Friendly, precise, and patient",
        values: ["accuracy", "helpfulness", "clarity"]
    },

    memory: {
        episodic: EpisodicStore::new(),      // Experiences
        semantic: SemanticStore::new(),       // Facts and knowledge
        procedural: ProceduralStore::new(),   // Skills and procedures
        working: WorkingMemory::new(7)        // Short-term context
    },

    beliefs: {
        revision_threshold: 0.3,
        contradiction_resolution: "evidence_weighted"
    },

    persistence: {
        path: "/data/assistant.anima",
        auto_save: true,
        interval: Duration::minutes(5)
    }
}
```

### Creating an Anima

```simplex
// Create a new anima
let soul = anima AssistantSoul

// Or with shorthand
let soul = anima {
    purpose: "Code review assistant",
    personality: "Thorough and constructive"
}

// Load existing anima
let soul = Anima::load("/data/assistant.anima")
```

---

## Memory Systems

The Anima has four distinct memory systems, mirroring human cognition:

### Episodic Memory

Stores autobiographical experiences - what happened, when, and in what context.

```simplex
// Remember an experience
soul.remember("User asked about error handling")
soul.remember("Helped debug a null pointer exception", importance: 0.9)

// Remember with context
soul.remember(Experience {
    content: "Reviewed user's authentication code",
    timestamp: now(),
    context: { file: "auth.sx", user: "alice" },
    importance: 0.8
})
```

**Operations:**
- `remember(content)` - Store an experience
- `remember(content, importance)` - Store with importance score (0.0-1.0)
- `recall_recent(n)` - Get n most recent memories
- `recall_by_time(start, end)` - Get memories in time range

### Semantic Memory

Stores facts, concepts, and learned knowledge.

```simplex
// Learn a fact
soul.learn("Simplex uses actors for concurrency")
soul.learn("The Result type has Ok and Err variants", confidence: 0.95)

// Learn with source attribution
soul.learn(Knowledge {
    content: "HashMap lookup is O(1) average case",
    confidence: 0.99,
    source: "documentation"
})
```

**Operations:**
- `learn(fact)` - Store a fact
- `learn(fact, confidence)` - Store with confidence score
- `knows(query)` - Check if knowledge exists
- `explain(topic)` - Retrieve explanation of topic

### Procedural Memory

Stores skills, procedures, and how-to knowledge.

```simplex
// Store a procedure
soul.store_procedure("handle_api_error", [
    "Check HTTP status code",
    "Log error details",
    "Determine if retryable",
    "Execute retry or fallback"
])

// Retrieve procedure
let steps = soul.get_procedure("handle_api_error")
```

### Working Memory

Short-term memory for current context. Limited capacity (default: 7 items).

```simplex
// Push to working memory
soul.working.push("Current task: review PR #123")
soul.working.push("User preference: verbose output")

// Working memory auto-manages capacity
// Oldest items drop when capacity exceeded

// Get current context
let context = soul.working.context()

// Clear working memory
soul.working.clear()
```

---

## Belief System

The Anima maintains beliefs that can be formed, revised, and contradicted.

### Forming Beliefs

```simplex
// Form a belief
soul.believe("User prefers concise responses", confidence: 0.7)

// Belief with evidence
soul.believe(Belief {
    content: "This codebase follows clean architecture",
    confidence: 0.8,
    evidence: ["Separated domain layer", "Dependency injection used"]
})
```

### Revising Beliefs

Beliefs can be strengthened or weakened with new evidence:

```simplex
// Revise belief based on new evidence
soul.revise_belief(
    "User prefers concise responses",
    new_confidence: 0.3,
    evidence: "User asked for more detailed explanations"
)
```

### Contradiction Resolution

When beliefs conflict:

```simplex
anima {
    beliefs: {
        // How to handle contradictions
        contradiction_resolution: "evidence_weighted",  // Default
        // Or: "recency_biased", "confidence_threshold", "ask_user"

        revision_threshold: 0.3  // Min evidence to revise
    }
}
```

---

## BDI Architecture

The Anima implements the Beliefs-Desires-Intentions model:

### Desires (Goals)

```simplex
// Add a desire (goal)
soul.desire("Help user understand error handling", priority: 0.8)
soul.desire("Maintain code quality standards", priority: 0.6)

// Check top desire
let top_goal = soul.top_desire()

// Mark desire as achieved
soul.achieve_desire("Help user understand error handling")
```

### Intentions (Active Plans)

```simplex
// Form an intention (active plan)
soul.intend(Intention {
    goal: "Review authentication module",
    plan: [
        "Read auth.sx",
        "Check for security issues",
        "Review error handling",
        "Suggest improvements"
    ],
    current_step: 0
})

// Advance to next step
soul.advance_intention("Review authentication module")

// Get current step
let step = soul.current_step("Review authentication module")
```

---

## Goal-Directed Recall

The Anima can recall memories relevant to current goals:

```simplex
// Recall memories relevant to a goal
let memories = soul.recall_for("error handling")

// Recall with context
let memories = soul.recall_for(
    goal: "debug null pointer",
    context: "authentication code",
    max_results: 10
)

// The anima searches across all memory types:
// - Episodic: Similar past experiences
// - Semantic: Relevant facts
// - Procedural: Applicable procedures
```

---

## Thinking with the Anima

The `think` operation uses an SLM for reasoning:

```simplex
// Think about a question
let answer = soul.think("What's the best approach for error handling here?")

// Think with context
let answer = soul.think(
    question: "Should I suggest refactoring?",
    context: soul.recall_for("refactoring decisions")
)
```

---

## Persistence

Anima state can be saved and loaded:

```simplex
// Manual save
soul.save("/data/assistant.anima")

// Auto-save configuration
anima {
    persistence: {
        path: "/data/assistant.anima",
        auto_save: true,
        interval: Duration::minutes(5)
    }
}

// Load existing anima
let soul = Anima::load("/data/assistant.anima")

// Check if save exists
if Anima::exists("/data/assistant.anima") {
    let soul = Anima::load("/data/assistant.anima")
} else {
    let soul = anima AssistantSoul
}
```

---

## Sharing Anima

Multiple actors can share an anima:

```simplex
// Create shared anima
let shared_soul = anima TeamMemory

// Create read-write view
let rw_view = shared_soul.view(ReadWrite)

// Create read-only view
let ro_view = shared_soul.view(ReadOnly)

// Use in actors
actor Worker {
    anima: ro_view,  // Can read but not modify

    receive Process(task: Task) {
        let context = self.anima.recall_for(task.description)
        // Process with context...
    }
}

actor Learner {
    anima: rw_view,  // Can read and write

    receive Learn(experience: String) {
        self.anima.remember(experience)
    }
}
```

---

## Memory Consolidation

The Anima automatically consolidates memories:

```simplex
// Manual consolidation
let pruned = soul.consolidate()
print("Pruned {pruned} low-importance memories")

// Auto-consolidation settings
anima {
    memory: {
        consolidation: {
            // Prune memories below this importance
            importance_threshold: 0.3,

            // Run consolidation every N memories
            interval: 1000,

            // Keep at least this many memories
            min_episodic: 100,
            min_semantic: 500
        }
    }
}
```

Consolidation:
- Removes low-importance episodic memories
- Strengthens frequently-accessed semantic knowledge
- Extracts patterns from experiences into semantic facts

---

## Integration with Actors

### Cognitive Actor

```simplex
actor CognitiveAssistant {
    anima: AssistantSoul,
    tools: ToolRegistry,
    specialist: CodeAnalyzer,

    receive Chat(message: String) -> String {
        // Remember the interaction
        self.anima.remember("User: {message}")

        // Recall relevant context
        let context = self.anima.recall_for(message)

        // Think about the response
        let response = self.anima.think(
            "How should I respond to: {message}",
            context: context
        )

        // Learn from the interaction
        if response.confidence > 0.8 {
            self.anima.learn(
                "Successful response pattern for: {message.category()}",
                confidence: response.confidence
            )
        }

        response.content
    }
}
```

### Team with Shared Memory

```simplex
// Create team with shared anima
let team_memory = anima TeamKnowledge

actor Researcher {
    anima: team_memory.view(ReadWrite),

    receive Research(topic: String) {
        let findings = do_research(topic)
        self.anima.learn(findings)
        self.anima.remember("Researched: {topic}")
    }
}

actor Synthesizer {
    anima: team_memory.view(ReadOnly),

    receive Synthesize(query: String) -> Report {
        let knowledge = self.anima.recall_for(query)
        generate_report(knowledge)
    }
}
```

---

## Anima and Hive Integration (v0.5.0)

### The Memory Hierarchy

In v0.5.0, memories flow through three levels:

```
Specialist Anima (Personal)
         │
         ▼
   HiveMnemonic (Shared)
         │
         ▼
    Divine SLM (Global)
```

**Belief thresholds at each level**:
- **Anima**: 30% (individual beliefs, flexible)
- **Mnemonic**: 50% (shared beliefs, need consensus)
- **Divine**: 70% (global beliefs, high confidence)

### How Anima Feeds into Hive SLM

When a specialist calls `infer()` or `think()`, the context is assembled:

```simplex
specialist Analyst {
    receive Analyze(text: String) -> String {
        // When infer() is called:
        // 1. Format Anima memories (episodic, semantic, beliefs)
        // 2. Add Hive Mnemonic context
        // 3. Append the prompt
        // 4. Send to shared Hive SLM

        infer("Analyze: " + text)
    }
}
```

### Context Format

```
<context>
Recent experiences:
- Analyzed security code yesterday
- Found SQL injection in login.sx

Known facts:
- This codebase uses prepared statements
- Team prefers explicit error handling

Current beliefs (confidence > 30%):
- User prefers detailed output (85%)
</context>

<hive name="SecurityHive">
Shared experiences:
- Team completed security audit last week

Shared knowledge:
- Production uses PostgreSQL 15

Hive beliefs (confidence > 50%):
- Always validate user input (92%)
</hive>

Analyze: [user's text here]
```

### Contributing to Shared Memory

Specialists can contribute to both personal and shared memory:

```simplex
specialist Researcher {
    receive Research(topic: String) {
        let findings = do_research(topic)

        // Personal memory (my Anima)
        self.anima.remember("I researched: {topic}")
        self.anima.learn("Finding: {findings.key_insight}")

        // Shared memory (Hive Mnemonic)
        hive.mnemonic.learn("Research result: {findings.summary}")
        hive.mnemonic.believe(
            "Topic {topic} is well-documented",
            confidence: 80
        )

        findings
    }
}
```

### Anima in Hive Definition

```simplex
hive AnalyticsHive {
    specialists: [Analyzer, Summarizer, Critic],

    // Shared SLM for all specialists
    slm: "simplex-cognitive-7b",

    // Shared consciousness
    mnemonic: {
        episodic: { capacity: 1000 },
        semantic: { capacity: 5000 },
        beliefs: { revision_threshold: 50 },
    },
}

specialist Analyzer {
    // Anima configuration for this specialist
    anima: {
        purpose: "Analyze code for issues",
        beliefs: { revision_threshold: 30 },
    },

    receive Analyze(code: String) -> Analysis {
        // My Anima + Hive Mnemonic context flows to Hive SLM
        infer("Find issues in: " + code)
    }
}
```

---

## Runtime Functions

| Function | Description |
|----------|-------------|
| `anima_memory_new(capacity)` | Create new anima memory |
| `anima_remember(mem, content, importance)` | Store episodic memory |
| `anima_learn(mem, content, confidence, source)` | Store semantic memory |
| `anima_store_procedure(mem, name, steps)` | Store procedural memory |
| `anima_believe(mem, content, confidence, evidence)` | Form belief |
| `anima_revise_belief(mem, id, confidence, evidence)` | Revise belief |
| `anima_working_push(mem, item)` | Push to working memory |
| `anima_working_pop(mem)` | Pop from working memory |
| `anima_recall_for_goal(mem, goal, context, max)` | Goal-directed recall |
| `anima_consolidate(mem)` | Consolidate memories |
| `anima_save(mem, path)` | Save to file |
| `anima_load(path)` | Load from file |
| `anima_memory_close(mem)` | Close and cleanup |
| `hive_mnemonic_learn(hive, content)` | Add to shared knowledge (v0.5.0) |
| `hive_mnemonic_recall(hive, query)` | Recall from shared memory (v0.5.0) |

---

## Best Practices

### 1. Appropriate Importance Scores

```simplex
// High importance (0.8-1.0): Critical decisions, errors, user preferences
soul.remember("User explicitly requested verbose output", importance: 0.9)

// Medium importance (0.4-0.7): Normal interactions, facts learned
soul.remember("Answered question about async", importance: 0.5)

// Low importance (0.1-0.3): Routine events, will be pruned
soul.remember("Started new session", importance: 0.2)
```

### 2. Use Working Memory for Context

```simplex
receive ProcessTask(task: Task) {
    // Set up working memory context
    self.anima.working.clear()
    self.anima.working.push("Task: {task.description}")
    self.anima.working.push("Priority: {task.priority}")

    // Process with context available
    let result = process(task)

    // Remember outcome
    self.anima.remember("Completed task: {task.description}")
}
```

### 3. Belief Revision with Evidence

```simplex
// Always provide evidence when revising beliefs
soul.revise_belief(
    belief_id,
    new_confidence: 0.8,
    evidence: "User confirmed this approach works"
)
```

### 4. Regular Consolidation

```simplex
// Consolidate after batch operations
for item in large_batch {
    soul.remember(item)
}
soul.consolidate()
```

---

## Summary

| Concept | Purpose |
|---------|---------|
| `anima` | Cognitive soul with memory, beliefs, intentions |
| `remember` | Store episodic experiences |
| `learn` | Store semantic knowledge |
| `believe` | Form revisable beliefs |
| `desire` | Set goals |
| `intend` | Form active plans |
| `recall_for` | Goal-directed memory retrieval |
| `think` | SLM-powered reasoning |
| `consolidate` | Memory maintenance |

The Anima gives Simplex AI agents:
- **Continuity**: Memory persists across sessions
- **Learning**: Agents improve from experience
- **Personality**: Consistent behavior and values
- **Purpose**: Goal-directed behavior
- **Adaptability**: Beliefs update with evidence

---

*"Every soul has a memory, every memory shapes the soul."*

---

*See also: [SLM Provisioning](13-slm-provisioning.md) | [Cognitive Hive AI](09-cognitive-hive.md) | [AI Integration](07-ai-integration.md) | [Actors](../tutorial/07-actors.md)*
