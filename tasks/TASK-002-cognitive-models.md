# TASK-002: Simplex Cognitive Models

**Status**: Infrastructure Complete (~75%) — actual LLM training out of scope
**Priority**: High
**Created**: 2026-01-08
**Updated**: 2026-03-16 (audited against codebase)
**Target Version**: 0.6.0+

> **Audit Note (2026-03-16):** All cognitive model infrastructure is implemented in pure Simplex:
> - Belief system with provenance: `simplex-learning/src/belief/` (confidence, evidence, grounded, scope, timestamps)
> - Calibrated confidence with ECE/Brier: `simplex-learning/src/calibration/`
> - Epistemic annealing & belief revision: `simplex-learning/src/epistemic/`
> - LoRA adapters: `simplex-training/src/lora/`
> - Neural gates (soft logic, temperature-aware): `simplex-training/src/neural/`
> - Specialist training data generators: `simplex-training/src/data/specialists/` (6 domains)
> - Specialist trainer with distillation/compression: `simplex-training/src/trainer/`
> - Federated/distributed learning: `simplex-learning/src/distributed/`
> - Edge deployment: `simplex-edge-hive/src/specialist.sx`
>
> NOT implemented (out of scope for language toolchain):
> - Actual LLM models (Qwen 2.5 32B/7B/1.5B) — requires external compute
> - Ollama/GGUF export tooling — external dependency
> - Large-scale dataset processing — only synthetic generators exist

---

## Overview

Define and build the simplex-cognitive model family - the SLMs that power Cognitive Hive AI. These models are specifically trained for:

- Confidence-calibrated outputs
- Belief revision and soft logic
- Memory context protocol (Anima/Mnemonic)
- Neural IR/Gates compatibility (v0.6.0)

### Model Family

| Model | Parameters | Size (FP16) | Size (Q4) | Use Case |
|-------|-----------|-------------|-----------|----------|
| simplex-cognitive-30b | ~32B | 64 GB | 18 GB | Divine tier, cross-hive reasoning, complex synthesis |
| simplex-cognitive-7b | ~7B | 14 GB | 4.1 GB | Primary hive SLM, specialist inference |
| simplex-cognitive-1b | ~1.5B | 3 GB | 700 MB | Edge/mobile, fast inference, IoT |
| simplex-mnemonic-embed | ~110M | 220 MB | 134 MB | Memory recall, semantic routing |

### Model Hierarchy Mapping

```
┌─────────────────────────────────────────────────────────────┐
│                    DIVINE TIER                               │
│              simplex-cognitive-30b                           │
│   - Cross-hive synthesis and arbitration                    │
│   - Complex multi-step reasoning                            │
│   - Global belief management (70% threshold)                │
│   - Used sparingly for high-stakes decisions                │
└─────────────────────────┬───────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────────┐
│                    HIVE TIER                                 │
│              simplex-cognitive-7b                            │
│   - Primary SLM for each hive                               │
│   - Specialist inference (summarize, extract, analyze)      │
│   - Hive belief management (50% threshold)                  │
│   - Runs on commodity GPU (RTX 3090/4090, A10)             │
└─────────────────────────┬───────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────────┐
│                    EDGE TIER                                 │
│              simplex-cognitive-1b                            │
│   - Mobile and embedded deployment                          │
│   - Fast local inference (< 100ms)                          │
│   - Anima-level processing (30% threshold)                  │
│   - Runs on CPU, Apple Silicon, edge TPU                    │
└─────────────────────────────────────────────────────────────┘
```

---

## Phase 1: Base Model Selection

### Candidates for simplex-cognitive-30b (Divine Tier)

| Model | Params | Context | Strengths | Weaknesses | License |
|-------|--------|---------|-----------|------------|---------|
| **Qwen 2.5 32B** | 32B | 128K | Excellent reasoning, multilingual, long context | Resource intensive | Apache 2.0 |
| **Llama 3.1 70B** | 70B | 128K | State-of-art open model, massive community | Very large, overkill? | Llama 3.1 |
| **Mixtral 8x7B** | 47B (13B active) | 32K | MoE efficiency, good reasoning | Sparse, complex deployment | Apache 2.0 |
| **DeepSeek-V2** | 236B (21B active) | 128K | MoE, very efficient for size | Chinese origin concerns | MIT |
| **Command R+** | 104B | 128K | RAG-optimized, tool use | Cohere license |  CC-BY-NC |

**Recommendation**: **Qwen 2.5 32B** as primary candidate:
- True 32B dense model (not MoE complexity)
- 128K context for massive memory context
- Apache 2.0 license
- Same family as 7B/1.5B for consistency
- Strong multilingual and reasoning

**Alternative**: **Mixtral 8x22B** for better efficiency if MoE is acceptable.

### Candidates for simplex-cognitive-7b (Hive Tier)

| Model | Params | Context | Strengths | Weaknesses | License |
|-------|--------|---------|-----------|------------|---------|
| **Mistral 7B v0.3** | 7.3B | 32K | Strong reasoning, sliding window attention, good instruction following | Older architecture | Apache 2.0 |
| **Qwen 2.5 7B** | 7.6B | 128K | Excellent multilingual, strong code/math, recent | Chinese company concerns for some users | Apache 2.0 |
| **Llama 3.1 8B** | 8B | 128K | Massive community, well-documented, Meta backing | Slightly larger | Llama 3.1 Community |
| **Gemma 2 9B** | 9B | 8K | Google quality, efficient inference | Larger, shorter context | Gemma |

**Recommendation**: **Qwen 2.5 7B** as primary candidate:
- 128K context window (important for Anima/Mnemonic context)
- Strong reasoning and instruction following
- Apache 2.0 license allows commercial use
- Same family as 32B for consistent behavior
- Active development and community

**Fallback**: Mistral 7B v0.3 for simpler deployment.

### Candidates for simplex-cognitive-1b (Edge Tier)

| Model | Params | Context | Strengths | License |
|-------|--------|---------|-----------|---------|
| **Qwen 2.5 1.5B** | 1.5B | 128K | Punches above weight, same family as 7B | Apache 2.0 |
| **SmolLM 1.7B** | 1.7B | 2K | HuggingFace, efficient, well-documented | Apache 2.0 |
| **Phi-3.5 Mini** | 3.8B | 128K | Microsoft quality, strong reasoning | MIT |

**Recommendation**: **Qwen 2.5 1.5B** for consistency with 7B model family.

### Candidates for simplex-mnemonic-embed

| Model | Params | Dimensions | Strengths | License |
|-------|--------|------------|-----------|---------|
| **all-MiniLM-L6-v2** | 22M | 384 | Fast, proven, small | Apache 2.0 |
| **bge-small-en-v1.5** | 33M | 384 | BAAI, strong retrieval | MIT |
| **nomic-embed-text-v1.5** | 137M | 768 | Matryoshka, variable dims | Apache 2.0 |
| **gte-small** | 33M | 384 | Alibaba, strong performance | MIT |

**Recommendation**: **nomic-embed-text-v1.5** due to:
- Matryoshka embeddings (can use 384 or 768 dims as needed)
- Strong retrieval performance
- Good for semantic routing

---

## Phase 2: Training Pipeline

### Stage 1: Context Protocol Training

Train the model to understand and use Simplex memory context format:

```
<context>
Recent experiences:
- [episodic memory entries]

Known facts:
- [semantic memory entries]

Current beliefs (confidence > 30%):
- [belief] ([confidence]%)
</context>

<hive name="HiveName">
Shared experiences:
- [hive episodic entries]

Shared knowledge:
- [hive semantic entries]

Hive beliefs (confidence > 50%):
- [belief] ([confidence]%)
</hive>

[User prompt here]
```

**Training Data**: Synthetic conversations with memory context, ~100K examples

### Stage 2: Confidence Calibration

Train the model to output well-calibrated confidence scores.

**Approach 1: Explicit Confidence Training**
```
Input: "What is the capital of France?"
Output: "Paris [confidence: 0.99]"

Input: "Will it rain tomorrow in Sydney?"
Output: "Based on typical patterns, likely yes [confidence: 0.62]"
```

**Approach 2: Confidence Head**
Add a separate linear head that outputs P(correct) given the hidden state.

**Training Data**:
- Factual QA with ground truth (for high confidence)
- Ambiguous questions (for medium confidence)
- Unknowable questions (for low confidence)
- Calibration datasets (temperature scaling)

**Evaluation**: Expected Calibration Error (ECE) < 0.05

### Stage 3: Belief Revision Training

Train the model to update beliefs given new evidence.

**Scenario Format**:
```
Initial belief: "The project deadline is Friday" (confidence: 80%)
New evidence: "Manager mentioned deadline moved to Monday in standup"
Revised belief: "The project deadline is Monday" (confidence: 85%)
Reasoning: Direct statement from authority supersedes previous understanding
```

**Training Data**:
- Synthetic belief revision scenarios (~50K)
- Debate/argument datasets (ChangeMyView, Kialo)
- Scientific paper rebuttals
- News correction datasets

### Stage 4: Specialist LoRA Adapters

Train swappable LoRA adapters for different specialist domains:

| Adapter | Rank | Target Modules | Training Data |
|---------|------|----------------|---------------|
| summarization | 16 | q_proj, v_proj | CNN/DM, XSum, Multi-News |
| entity-extraction | 16 | q_proj, v_proj | CoNLL-2003, OntoNotes, Few-NERD |
| sentiment | 8 | q_proj, v_proj | SST-2, Amazon Reviews, Twitter |
| code | 32 | q_proj, k_proj, v_proj | The Stack, CodeSearchNet |
| reasoning | 16 | q_proj, v_proj | GSM8K, ARC, HellaSwag |

**LoRA Configuration**:
```python
lora_config = LoraConfig(
    r=16,
    lora_alpha=32,
    target_modules=["q_proj", "v_proj"],
    lora_dropout=0.05,
    bias="none",
    task_type="CAUSAL_LM"
)
```

---

## Phase 3: Neural IR/Gates Compatibility (v0.6.0)

For Neural IR support, the model needs special training considerations:

### Temperature-Aware Training

Train at multiple temperatures so the model understands soft vs hard decisions:

```python
temperatures = [0.1, 0.5, 1.0, 2.0, 5.0]  # During training
# Model learns that higher temp = more exploration/uncertainty
```

### Soft Logic Understanding

Train on examples with explicit threshold semantics:

```
Input: "Given confidence 0.45, should this pass a 50% threshold?"
Output: "No, 0.45 < 0.50, threshold not met [confidence: 0.99]"

Input: "Given confidence 0.72, should this pass a 70% threshold?"
Output: "Yes, 0.72 > 0.70, threshold met [confidence: 0.95]"
```

### Gradient-Friendly Outputs

For Gumbel-Softmax compatibility, train the model to:

1. Output logits/probabilities explicitly when asked
2. Understand categorical distributions over options
3. Handle "soft" selection between choices

```
Input: "Choose between Option A (code review) and Option B (testing). Output probabilities."
Output: "Option A: 0.65, Option B: 0.35"
```

### Straight-Through Estimator Awareness

Train on scenarios where the model must make hard decisions but explain soft reasoning:

```
Input: "Hard decision required: approve or reject. Confidence threshold: 70%"
Context: "Analysis confidence: 68%"
Output: "Decision: REJECT (hard). Reasoning: 68% < 70% threshold, but close. Consider: [soft factors...]"
```

---

## Phase 4: Dataset Curation

### Truth Datasets (High Confidence Facts)

| Dataset | Size | Purpose | Source |
|---------|------|---------|--------|
| Wikipedia (cleaned) | 6M articles | Factual knowledge | Wikimedia dumps |
| Natural Questions | 307K | Factual QA | Google |
| TriviaQA | 650K | Trivia facts | Joshi et al. |
| SQuAD 2.0 | 150K | Reading comprehension | Stanford |
| HotpotQA | 113K | Multi-hop reasoning | Yang et al. |

### Belief Datasets (Revisable, Contextual)

| Dataset | Size | Purpose | Source |
|---------|------|---------|--------|
| ChangeMyView | 3M posts | Belief revision | Reddit |
| Kialo | 50K debates | Argument structure | Kialo |
| FEVER | 185K | Fact verification | Thorne et al. |
| VitaminC | 450K | Contrastive claims | Schuster et al. |
| Synthetic Belief Revision | 100K | Custom scenarios | Generated |

### Intention Datasets (Goal-Directed)

| Dataset | Size | Purpose | Source |
|---------|------|---------|--------|
| MultiWOZ | 10K dialogues | Task-oriented dialogue | Budzianowski et al. |
| Schema-Guided Dialogue | 20K | Intent/slot filling | Google |
| ATIS | 5K | Intent classification | Hemphill et al. |
| SNIPS | 14K | Intent classification | Coucke et al. |
| Taskmaster | 30K | Goal-oriented | Google |

### Specialist Domain Datasets

**Code:**
| Dataset | Size | Purpose |
|---------|------|---------|
| The Stack v2 | 3TB | Code understanding |
| CodeSearchNet | 2M | Code-text pairs |
| HumanEval | 164 | Code generation eval |
| MBPP | 974 | Python problems |

**Summarization:**
| Dataset | Size | Purpose |
|---------|------|---------|
| CNN/DailyMail | 300K | News summarization |
| XSum | 227K | Extreme summarization |
| Multi-News | 56K | Multi-doc summarization |
| arXiv | 215K | Scientific summarization |

**Entity Extraction:**
| Dataset | Size | Purpose |
|---------|------|---------|
| CoNLL-2003 | 22K | Standard NER |
| OntoNotes 5.0 | 1.7M | Rich annotations |
| Few-NERD | 188K | Fine-grained NER |
| WikiANN | 40 langs | Multilingual NER |

**Sentiment:**
| Dataset | Size | Purpose |
|---------|------|---------|
| SST-2 | 70K | Sentence sentiment |
| Amazon Reviews | 233M | Product reviews |
| Yelp | 6.9M | Business reviews |
| Twitter Sentiment | 1.6M | Social media |

---

## Phase 5: Evaluation Framework

### Core Metrics

| Metric | Target | Description |
|--------|--------|-------------|
| ECE (Expected Calibration Error) | < 0.05 | Confidence calibration |
| Brier Score | < 0.15 | Probabilistic accuracy |
| MMLU | > 60% | General knowledge |
| HumanEval | > 40% | Code generation |
| MT-Bench | > 7.0 | Instruction following |

### Simplex-Specific Benchmarks

**Belief Revision Benchmark**:
- Accuracy on belief update scenarios
- Appropriate confidence adjustment
- Resistance to bad evidence

**Memory Context Benchmark**:
- Retrieval accuracy from provided context
- Appropriate use of episodic vs semantic
- Confidence adjustment based on memory

**Threshold Decision Benchmark**:
- Accuracy on threshold comparisons
- Soft logic reasoning
- Hard decision making with soft justification

---

## Phase 6: Training Infrastructure

### Compute Requirements

| Model | Full Training | LoRA Fine-tuning | Inference (Q4) |
|-------|--------------|------------------|----------------|
| simplex-cognitive-30b | 32x A100 80GB, ~2 weeks | 4x A100, ~24 hours | 1x A100 or 2x RTX 4090 |
| simplex-cognitive-7b | 8x A100 80GB, ~1 week | 1x A100, ~8 hours | 1x RTX 3090/4090 |
| simplex-cognitive-1b | 4x A100, ~2 days | 1x A100, ~2 hours | CPU / Apple M1+ |
| simplex-mnemonic-embed | 1x A100, ~4 hours | 1x A100, ~1 hour | CPU |

### Hardware Recommendations by Tier

**Divine Tier (30B) Deployment:**
- Cloud: 1x A100 80GB or 2x A10 24GB
- On-prem: 2x RTX 4090 24GB (NVLink preferred)
- Memory: 48GB+ system RAM
- Use case: Central arbitration server, batch processing

**Hive Tier (7B) Deployment:**
- Cloud: 1x T4 16GB or 1x A10 24GB
- On-prem: 1x RTX 3090/4090 or Apple M2 Pro+
- Memory: 16GB+ system RAM
- Use case: Per-hive SLM, always running

**Edge Tier (1B) Deployment:**
- Cloud: CPU instance (c6i.xlarge)
- On-prem: Any modern CPU, Apple M1+, Raspberry Pi 5
- Memory: 4GB+ system RAM
- Use case: Mobile apps, IoT, offline processing

### Training Stack

```
- Framework: PyTorch 2.x + Transformers
- Fine-tuning: PEFT (LoRA)
- Distributed: DeepSpeed ZeRO-3 or FSDP
- Monitoring: Weights & Biases
- Data: HuggingFace Datasets
- Quantization: bitsandbytes (for inference)
```

### Quantization Strategy

| Format | Size Reduction | Use Case |
|--------|---------------|----------|
| FP16 | 50% | Training, high-quality inference |
| INT8 | 75% | Server inference |
| INT4 (GPTQ/AWQ) | 87.5% | Edge deployment |
| GGUF Q4_K_M | ~85% | Ollama/llama.cpp |

---

## Phase 7: Deployment

### Ollama Model Cards

```yaml
# simplex-cognitive-7b.yaml
FROM qwen2.5:7b-instruct

PARAMETER temperature 0.7
PARAMETER num_ctx 32768

SYSTEM """
You are a Simplex cognitive specialist. You process information with calibrated confidence,
can revise beliefs given evidence, and understand the Anima/Mnemonic memory protocol.
Always output confidence scores when appropriate.
"""
```

### Model Registry

```
# Divine Tier (30B)
simplex-cognitive-30b          # Full 32B model (FP16)
simplex-cognitive-30b:q8       # 8-bit quantized (~32GB)
simplex-cognitive-30b:q4       # 4-bit quantized (~18GB)

# Hive Tier (7B)
simplex-cognitive-7b           # Full 7B model (FP16)
simplex-cognitive-7b:q8        # 8-bit quantized (~7GB)
simplex-cognitive-7b:q4        # 4-bit quantized (~4GB)

# Edge Tier (1B)
simplex-cognitive-1b           # Full 1.5B model (FP16)
simplex-cognitive-1b:q8        # 8-bit quantized (~1.5GB)
simplex-cognitive-1b:q4        # 4-bit quantized (~700MB)

# Embedding
simplex-mnemonic-embed         # Embedding model (384/768 dim)

# LoRA Adapters (compatible with 7B and 30B)
simplex-lora-summarize         # Summarization specialist
simplex-lora-code              # Code specialist
simplex-lora-sentiment         # Sentiment specialist
simplex-lora-entity            # Entity extraction specialist
simplex-lora-reasoning         # Complex reasoning specialist
simplex-lora-dialogue          # Conversational specialist
```

### Tier Selection Logic

```simplex
// Automatic tier selection based on task complexity
fn select_model_tier(task: Task, available_hardware: Hardware) -> ModelTier {
    match (task.complexity, task.confidence_required, available_hardware) {
        // Divine tier for cross-hive, high-stakes, or complex synthesis
        (Complexity::High, conf, _) if conf > 0.9 => ModelTier::Divine,
        (_, _, hw) if task.requires_arbitration => ModelTier::Divine,

        // Edge tier for simple, fast, or offline tasks
        (Complexity::Low, _, _) => ModelTier::Edge,
        (_, _, Hardware::Mobile) => ModelTier::Edge,
        (_, _, Hardware::Embedded) => ModelTier::Edge,

        // Hive tier is the default
        _ => ModelTier::Hive,
    }
}
```

---

## Licensing Considerations

**This work may be separately licensed from the open-source Simplex language.**

Options:
1. **Dual License**: Open weights for research, commercial license for production
2. **Tiered Access**: Base model open, specialist LoRAs commercial
3. **Hosted Only**: Models only available via Simplex Cloud API
4. **Full Commercial**: All models require commercial license

**Recommendation**: Dual license with open base model, commercial specialist adapters.

---

## Timeline

| Phase | Duration | Deliverable |
|-------|----------|-------------|
| Base Model Selection | 1 week | Decision document |
| Dataset Curation | 2-3 weeks | Cleaned, formatted datasets |
| Stage 1 Training (Context) | 1 week | Context-aware base model |
| Stage 2 Training (Confidence) | 1 week | Calibrated model |
| Stage 3 Training (Belief) | 1 week | Belief revision capability |
| Stage 4 Training (LoRA) | 2 weeks | Specialist adapters |
| Evaluation | 1 week | Benchmark results |
| Quantization & Packaging | 1 week | Ollama-ready models |

**Total**: ~10-12 weeks for full model family

---

## Open Questions

1. **Context Window**: How much of 128K context to allocate to Anima vs Mnemonic vs prompt?
2. **Confidence Format**: Inline `[confidence: 0.85]` vs separate field vs logits?
3. **LoRA Stacking**: Can we stack multiple specialist adapters? (e.g., code + summarization)
4. **Continuous Learning**: How do we update models as Anima learns? (Titan-style?)
5. **Privacy**: Can we train on user data with differential privacy?

---

## References

- Guo et al., "On Calibration of Modern Neural Networks" (2017)
- Desai & Durrett, "Calibration of Pre-trained Transformers" (2020)
- Hu et al., "LoRA: Low-Rank Adaptation" (2021)
- Touvron et al., "Llama 2" (2023)
- Bai et al., "Qwen Technical Report" (2023)
- Jiang et al., "Mistral 7B" (2023)

---

*This document is CONFIDENTIAL and may be subject to separate commercial licensing.*
