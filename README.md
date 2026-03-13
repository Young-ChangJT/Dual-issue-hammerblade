# Dual-issue-hammerblade

This repository is a HammerBlade / BSG Bladerunner–based workspace that is intended to **stress and experiment with a dual-issue microarchitecture structure** (i.e., enabling/validating a pipeline capable of issuing two operations per cycle under appropriate conditions).

The upstream HammerBlade/Bladerunner environment is vendored under [`bsg_bladerunner/`](bsg_bladerunner/) and provides the build + simulation flows (Synopsys VCS and/or Verilator) and the supporting submodules (e.g., `bsg_manycore`, `bsg_replicant`, `basejump_stl`).

> For baseline environment setup, prerequisites, and build targets, start here:  
> **[`bsg_bladerunner/README.md`](bsg_bladerunner/README.md)**

---

## What this structure emphasizes: Dual-issue structure

This repo’s focus is to support **dual-issue** development/debugging, such as:

- **Issue width = 2**: exploring when the core can issue two operations in the same cycle
- **Hazard handling**: structural / data / control hazards that may reduce issue rate
- **Scheduler/scoreboard behavior** (depending on the implementation): ensuring correct dependency tracking
- **Validation via simulation**: running kernels/tests and inspecting performance/correctness impacts

### How to confirm “dual-issue” is active (recommended checklist)

Because the exact switch/parameter name can vary by implementation, use this checklist to confirm your configuration is truly running in dual-issue mode:

1. **Locate the dual-issue parameter / macro** (often in RTL `parameters`, `localparam`, or `define`s).
2. **Run a small simulation** and confirm from:
   - build logs (parameter echo),
   - waveform inspection,
   - or a counter/trace that shows **2 instructions issued in a single cycle**.
3. **Add/enable assertions** that catch illegal dual-issue pairings (resource conflicts, RAW hazards, etc.).

> If you tell me the exact parameter name or file where you implemented dual-issue (e.g., `issue_width`, `dual_issue_p`, `PIPE_ISSUE_WIDTH`, etc.), I can rewrite this section to name the *exact knobs* and *exact commands*.

---

## Repository layout

- `bsg_bladerunner/` — main project directory (Makefiles, scripts, and submodules)
  - `README.md` — upstream primary docs (overview, requirements, setup)
  - `Makefile` — common build / setup targets (`make help`)
  - `amibuild.mk` — toolchain/build setup targets
  - `project.mk` — paths to submodule dependencies
  - `AWS.md` — AWS flow notes (deprecated upstream, kept for reference)
  - `scripts/` — helper scripts
  - `aws-fpga/`, `basejump_stl/`, `bsg_manycore/`, `bsg_replicant/`, `verilator/` — upstream components (often submodules)

---

## Quick start (from repo root)

```bash
git clone https://github.com/Young-ChangJT/Dual-issue-hammerblade.git
cd Dual-issue-hammerblade

cd bsg_bladerunner
git submodule update --init --recursive
```

Then follow:
- [`bsg_bladerunner/README.md`](bsg_bladerunner/README.md)

---

## Notes

- AWS FPGA support is noted as deprecated upstream, but files remain for reference (see `bsg_bladerunner/README.md`).
- VCS-based simulation requires Synopsys VCS (available on `PATH`) for that flow.

---

## License

See [`bsg_bladerunner/LICENSE`](bsg_bladerunner/LICENSE).