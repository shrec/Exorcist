# Exorcist Development Principles

These principles define how Exorcist evolves as a system. They guide every decision — from architecture choices to daily development workflow.

---

## 1. Horizontal Evolution

Exorcist evolves **horizontally**.
No subsystem should develop in isolation.

Every iteration improves a little across:

- editor
- agent
- tooling
- plugin system
- UI
- project model
- runtime

The goal is a **balanced system**, not perfection in one place.

---

## 2. Working System First

A **working system** comes before ideal architecture.

We don't wait for the perfect design before shipping functionality.

```
make it work
→ make it usable
→ make it clean
```

---

## 3. Dogfooding

Exorcist must always be used **for its own development**.

The IDE is its own test. If a feature isn't used in real work, its necessity is questionable.

---

## 4. Disposable Code

Code **is not sacred**.

Any part can be rewritten if:

- architecture improves
- simplicity increases
- dependencies decrease

Refactor and rewrite is a **normal process**.

---

## 5. Minimal Core

The core must remain **minimalist**.

Core is responsible only for:

- runtime
- plugin system
- project model
- orchestration

Everything else goes to:

```
plugins
services
tools
panels
```

See [core-philosophy.md](core-philosophy.md) for the full Core vs Plugin boundary.

---

## 6. On-Demand Activation

The system must not stay permanently loaded.

Components activate **on demand**:

```
open file     → language plugin activates
close file    → plugin idles
timeout       → plugin unloads
```

This protects:

- memory
- CPU
- startup speed

---

## 7. Capability Over Coupling

Subsystems must not know each other directly.

Communication happens through:

- services
- interfaces
- commands
- events

Dependencies are on **capabilities**, not concrete implementations.

---

## 8. System Over Features

Exorcist is not just an editor. It is:

```
development runtime
+ tools platform
+ agent environment
```

Features must serve **system integrity**, not just a feature checklist.

---

## 9. Evolution Through Use

Architecture is not shaped by design alone.

It is shaped by:

- real usage
- experimentation
- mistakes
- refactoring

---

## 10. Self-Improving Environment

The ultimate goal is an environment that helps **develop itself**.

The IDE must be:

```
a tool
for building
better tools
```

---

## The Formula

Exorcist grows like this:

```
build → use → observe → refine → repeat
```

---

These principles ensure decisions are made **systematically**, not emotionally — keeping the project stable as it scales.
