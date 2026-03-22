# TASK-050: Sparse Distributed Memory — Lifelong Learning

**Version:** 0.17.0
**Status:** Complete
**Priority:** P1 — High
**Depends on:** v0.16.0 release, Hopfield memory (TASK-047)

## Why This Feature Is Needed

Every deployed model today has the same fatal flaw: **it stops learning at deployment.**
The world changes, user behavior shifts, new information emerges — and the model is
frozen. Retraining is expensive, slow, and risks catastrophic forgetting (destroying old
knowledge while learning new). Fine-tuning is fragile. Online learning with gradient
descent is unstable.

Sparse Distributed Memory (SDM), introduced by Pentti Kanerva at NASA in 1988, is a
mathematical model of how the human brain stores and retrieves long-term memories. It
solves the lifelong learning problem through a fundamentally different approach:

- **High-dimensional binary addresses.** Memory locations are addressed by binary vectors
  in 1000+ dimensional space. At these dimensions, random vectors are almost orthogonal
  — stored memories rarely interfere.

- **Content-addressable retrieval.** You retrieve by similarity, not by exact address.
  Present a partial or noisy cue, and the memory returns the closest match — like human
  recall, where a smell triggers a complete memory.

- **Distributed storage.** Each memory is spread across many physical locations. Each
  location participates in storing many memories. This distribution makes the memory
  robust to noise, corruption, and partial failure.

- **Online, single-shot learning.** Store a new memory in one operation. No gradient
  descent, no backpropagation, no retraining. The memory is immediately available for
  retrieval.

Recent work (CALM, 2025) combines SDM with lightweight transformers for **continual
associative learning without catastrophic forgetting** — the exact capability that
deployed specialist SLMs need.

## Why It Adds Value

1. **True lifelong learning.** Specialists learn from every interaction without
   retraining. Each user interaction, each new data point, each corrected mistake
   becomes a stored memory immediately available for future retrieval.

2. **Zero catastrophic forgetting.** The high-dimensional sparse coding ensures new
   memories don't overwrite old ones. A medical specialist that learns about a new drug
   interaction doesn't forget existing pharmacology.

3. **Single-shot knowledge acquisition.** Tell the specialist something once — it
   remembers permanently. No few-shot prompting, no fine-tuning, no retraining pipeline.
   One interaction = one memory.

4. **Graceful degradation.** Because memories are distributed across many locations,
   partial corruption or hardware failure doesn't destroy memories — they degrade
   gracefully. Essential for edge deployment where hardware is unreliable.

5. **Brain-inspired architecture.** SDM's properties (sparse coding, content-addressable
   retrieval, distributed storage) mirror what neuroscience knows about human memory.
   This is not a metaphor — the mathematical properties are the same.

6. **Complementary to Hopfield.** Hopfield memory (TASK-047) provides fast associative
   retrieval for known patterns. SDM provides robust long-term storage with online
   learning. Together: fast retrieval + lifelong learning.

## Why It Changes Systems Built With Simplex

SDM transforms deployed specialists from static to living systems:

- **Specialists that get smarter every day.** Each user interaction creates memories.
  After a month of deployment, the specialist has accumulated thousands of domain-
  specific memories that improve its responses — without a single retraining cycle.

- **Personalized without fine-tuning.** Each user's interactions create a personal SDM
  layer. The specialist adapts to individual preferences and patterns through memory,
  not through weight changes.

- **Collective intelligence via memory sharing.** When SDM memories are federated across
  hive instances, knowledge discovered by any instance benefits all instances. A medical
  specialist in Hospital A learns about an unusual drug reaction; Hospital B's
  specialist retrieves this memory the next day.

- **Resilient edge deployment.** SDM's distributed nature means edge devices with
  unreliable storage still maintain functional memory. Partial data loss degrades
  quality smoothly instead of catastrophically.

## Deliverables

### Phase 1: SDM Core (~500 lines)

Location: `simplex-learning/src/sdm/`

- **SparseDistributedMemory** — the core SDM implementation: high-dimensional binary
  address space, hard location addresses, read/write operations
- **AddressEncoder** — encode real-valued data into high-dimensional binary addresses
- **DistributedStorage** — storage array where each location holds a counter vector,
  read returns the majority vote across activated locations
- **SimilarityRetrieval** — content-addressable retrieval within Hamming distance
  threshold

```simplex
/// Sparse Distributed Memory
struct SparseDistributedMemory {
    address_dim: usize,        // dimensionality of binary addresses (e.g., 1000)
    word_dim: usize,           // dimensionality of stored words
    num_locations: usize,      // number of hard locations (physical memory rows)
    hard_locations: Tensor,    // [num_locations x address_dim] binary
    counters: Tensor,          // [num_locations x word_dim] integer counters
    access_radius: usize,      // Hamming distance threshold for activation
}

impl SparseDistributedMemory {
    fn new(address_dim: usize, word_dim: usize, num_locations: usize, radius: usize) -> Self {
        // Hard locations are random binary vectors
        let hard_locations = Tensor::random_binary([num_locations, address_dim]);
        let counters = Tensor::zeros_int([num_locations, word_dim]);
        Self { address_dim, word_dim, num_locations, hard_locations, counters, access_radius: radius }
    }

    /// Find all hard locations within Hamming distance of address
    fn activated_locations(self: &Self, address: &Tensor) -> Vec<usize> {
        (0..self.num_locations)
            .filter(|&i| hamming_distance(&self.hard_locations.row(i), address) <= self.access_radius)
            .collect()
    }

    /// Write: store a word at an address (single-shot, no gradient)
    fn write(mut self: &mut Self, address: &Tensor, word: &Tensor) {
        let locations = self.activated_locations(address);
        for loc in locations {
            // Increment counters for +1 bits, decrement for -1 bits
            for j in 0..self.word_dim {
                if word.get(j) > 0.0 {
                    self.counters.add_at(loc, j, 1);
                } else {
                    self.counters.add_at(loc, j, -1);
                }
            }
        }
    }

    /// Read: retrieve word stored at address (returns majority vote)
    fn read(self: &Self, address: &Tensor) -> Tensor {
        let locations = self.activated_locations(address);
        let mut sum = Tensor::zeros([self.word_dim]);
        for loc in &locations {
            sum = sum + self.counters.row(*loc);
        }
        // Majority vote: positive sum → 1, negative → -1
        sum.sign()
    }

    /// Content-addressable retrieval: iterative read until convergence
    fn retrieve(self: &Self, cue: &Tensor, max_steps: usize) -> Tensor {
        let mut address = cue.clone();
        for _ in 0..max_steps {
            let retrieved = self.read(&address);
            if retrieved == address { break; }  // converged
            address = retrieved;
        }
        address
    }
}
```

Files:
- `simplex-learning/src/sdm/mod.sx` — module root
- `simplex-learning/src/sdm/memory.sx` — SparseDistributedMemory
- `simplex-learning/src/sdm/encoder.sx` — AddressEncoder (real → binary)
- `simplex-learning/src/sdm/storage.sx` — DistributedStorage
- `simplex-learning/src/sdm/retrieval.sx` — content-addressable retrieval

### Phase 2: Neural SDM Integration (~400 lines)

- **NeuralSDM** — SDM with learned address encoder (neural network maps inputs to
  high-dimensional binary addresses)
- **SDMLayer** — drop-in layer that reads from SDM based on input, augmenting model
  output with retrieved memories
- **SDMController** — controller that decides when to write (store new knowledge) vs
  when to read (retrieve existing knowledge)
- **DifferentiableSDM** — soft/relaxed version of SDM for end-to-end training (soft
  Hamming distance, soft majority vote)

```simplex
/// SDM with learned address encoding
struct NeuralSDM {
    encoder: Linear,           // maps input → binary address
    sdm: SparseDistributedMemory,
    write_threshold: f64,      // confidence threshold for storing new memories
}

impl NeuralSDM {
    /// Encode input to binary address via learned encoder
    fn encode(self: &Self, input: &Tensor) -> Tensor {
        self.encoder.forward(input.clone()).sign()  // binarize
    }

    /// Augmented forward: model input + retrieved memory context
    fn forward(self: &Self, input: &Tensor) -> (Tensor, Option<Tensor>) {
        let address = self.encode(input);
        let retrieved = self.sdm.retrieve(&address, 5);
        (input.clone(), Some(retrieved))
    }

    /// Learn from interaction: store if novel enough
    fn learn(mut self: &mut Self, input: &Tensor, output: &Tensor) {
        let address = self.encode(input);
        let existing = self.sdm.read(&address);
        let novelty = hamming_distance(&existing, &self.encode(output));
        if novelty as f64 / self.sdm.address_dim as f64 > self.write_threshold {
            self.sdm.write(&address, &self.encode(output));
        }
    }
}
```

Files:
- `simplex-learning/src/sdm/neural.sx` — NeuralSDM
- `simplex-learning/src/sdm/layer.sx` — SDMLayer
- `simplex-learning/src/sdm/controller.sx` — SDMController
- `simplex-learning/src/sdm/differentiable.sx` — soft SDM for training

### Phase 3: Hive Lifelong Learning (~400 lines)

- **SDMAnima** — specialist's personal SDM for lifelong experience accumulation
- **FederatedSDM** — federate memories across hive instances (share rare/important
  memories, keep common ones local)
- **MemoryConsolidation** — periodic consolidation of SDM memories (merge similar,
  prune stale) — inspired by sleep memory consolidation
- **InterleaveWithHopfield** — SDM for long-term storage, Hopfield for fast working
  memory; periodic transfer between the two

Files:
- `simplex-learning/src/sdm/anima.sx` — SDMAnima for specialists
- `simplex-learning/src/sdm/federated.sx` — FederatedSDM
- `simplex-learning/src/sdm/consolidation.sx` — memory consolidation
- `simplex-learning/src/sdm/interleave.sx` — SDM + Hopfield interleaving

### Phase 4: Tests (~350 lines)

Location: `tests/sdm/`

- Store and retrieve 100 binary patterns with >95% bit accuracy
- Content-addressable retrieval with 20% corruption recovers correct pattern
- Lifelong learning: 1000 sequential writes without degrading early memories
- NeuralSDM stores novel patterns and retrieves them in subsequent forward passes
- Federated SDM shares rare memories across instances
- Memory consolidation merges similar patterns without information loss

## Success Criteria

- [ ] Store 1000 patterns in 1000-dim SDM, retrieve with >90% bit accuracy
- [ ] 30% corrupted cue recovers correct pattern >85% of time
- [ ] No catastrophic forgetting: first 100 patterns retrievable after 900 more writes
- [ ] NeuralSDM augments model output with relevant retrieved memories
- [ ] Federated SDM converges to shared memory state across 4 simulated instances
- [ ] Write operation is O(1) — constant time regardless of memory size

## Estimated Scope

~1,650 lines across library code and tests.
