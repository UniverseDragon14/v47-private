# v47

v47 is a small quantum-language/simulator workspace. It includes `.qnc` examples and a C source file for quantum shot execution.

## What is in this repo

- `src/quantum/qshots.c` — C implementation for quantum shot/simulation logic.
- `examples/plus.qnc` — single-qubit `|+>` example.
- `examples/bell.qnc` — Bell-state example.
- `examples/ghz3.qnc` — GHZ 3-qubit example.
- `qnova-shots` — binary or non-UTF-8 executable/artifact.

## Example inputs

```text
# examples/plus.qnc
qubits 1
h 0
measure
```

```text
# examples/bell.qnc
qubits 2
h 0
cx 0 1
measure
```

## Current state

The repo contains the source/example pieces, but there is no root build script or Makefile yet. Add documented build commands when the compile workflow is finalized.
