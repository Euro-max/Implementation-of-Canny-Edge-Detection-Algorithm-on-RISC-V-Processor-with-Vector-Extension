#!/usr/bin/env python3
# Runs O0, O2, O3 binaries on QEMU and prints a combined comparison table
# Usage: python3 scripts/sweep_table.py <input.raw> <W> <H>

import subprocess, sys, re

# ── Config ──────────────────────────────────────────────────────
QEMU      = "qemu-riscv64"
QEMU_LIB  = "/usr/riscv64-linux-gnu"
BINARIES  = {
    "-O0": "build_rv/canny_O0",
    "-O2": "build_rv/canny_O2",
    "-O3": "build_rv/canny_O3",
}
STAGES = ["Gaussian", "Sobel", "Magnitude", "Direction"]

def run_binary(binary, raw_input, w, h):
    """Run one binary on QEMU and return its stdout"""
    cmd = [QEMU, "-L", QEMU_LIB, f"./{binary}",
           raw_input, "out_tmp.raw", str(w), str(h)]
    try:
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=120)
        return result.stdout
    except subprocess.TimeoutExpired:
        print(f"  ERROR: {binary} timed out!")
        return ""
    except FileNotFoundError:
        print(f"  ERROR: {binary} not found — run 'make sweep' first!")
        return ""

def parse_cycles(output):
    """
    Parse cycle counts from main.cpp output.
    Looks for lines like: '[ OK  ] Done — 654573884 cycles avg'
    Returns dict: {"Gaussian": 654573884, "Sobel": ..., ...}
    """
    cycles = {}
    # Pattern: '[ OK  ] Done — NUMBER cycles avg'
    pattern = re.compile(r'Done\s*[—-]\s*([\d,]+)\s*cycles')

    matches = pattern.findall(output)
    # Remove commas from numbers like "1,354,408,478"
    values = [int(m.replace(",", "")) for m in matches]

    # Map to stage names in order
    for i, stage in enumerate(STAGES):
        if i < len(values):
            cycles[stage] = values[i]
        else:
            cycles[stage] = 0

    return cycles

def print_comparison_table(all_results):
    """Print the combined comparison table"""

    flags = list(all_results.keys())   # ["-O0", "-O2", "-O3"]

    # ── Compute totals ──
    totals = {}
    for flag in flags:
        totals[flag] = sum(all_results[flag].values())

    # ── Column widths ──
    W_STAGE   = 12
    W_CYCLES  = 16
    W_PCT     = 8
    W_SPEEDUP = 10

    sep_line = (
        "├" + "─"*(W_STAGE+2) +
        ("┼" + "─"*(W_CYCLES+2) + "┼" + "─"*(W_PCT+2)) * len(flags) +
        "┼" + "─"*(W_SPEEDUP+2) + "┤"
    )

    top_line = (
        "┌" + "─"*(W_STAGE+2) +
        ("┬" + "─"*(W_CYCLES+2) + "┬" + "─"*(W_PCT+2)) * len(flags) +
        "┬" + "─"*(W_SPEEDUP+2) + "┐"
    )

    bot_line = (
        "└" + "─"*(W_STAGE+2) +
        ("┴" + "─"*(W_CYCLES+2) + "┴" + "─"*(W_PCT+2)) * len(flags) +
        "┴" + "─"*(W_SPEEDUP+2) + "┘"
    )

    print()
    print("  COMPILER OPTIMIZATION SWEEP — Results")
    print()
    print(top_line)

    # ── Header row ──
    header = f"│ {'Stage':<{W_STAGE}} "
    for flag in flags:
        header += f"│ {flag+' Cycles':>{W_CYCLES}} │ {'% total':^{W_PCT}} "
    header += f"│ {'O0→O3':^{W_SPEEDUP}} │"
    print(header)
    print(sep_line)

    # ── Stage rows ──
    for stage in STAGES:
        o0_cyc = all_results["-O0"].get(stage, 0)
        o3_cyc = all_results["-O3"].get(stage, 0)
        speedup = o0_cyc / o3_cyc if o3_cyc > 0 else 0

        row = f"│ {stage:<{W_STAGE}} "
        for flag in flags:
            cyc = all_results[flag].get(stage, 0)
            tot = totals[flag]
            pct = 100.0 * cyc / tot if tot > 0 else 0
            row += f"│ {cyc:>{W_CYCLES},} │ {pct:>{W_PCT-1}.1f}% "

        # Speedup column with color coding
        if speedup >= 10:
            sp_str = f"{speedup:.1f}×  🔥"
        elif speedup >= 5:
            sp_str = f"{speedup:.1f}×  ✓"
        else:
            sp_str = f"{speedup:.1f}×"

        row += f"│ {sp_str:^{W_SPEEDUP}} │"
        print(row)

    print(sep_line)

    # ── Total row ──
    o0_total = totals.get("-O0", 1)
    o3_total = totals.get("-O3", 1)
    total_speedup = o0_total / o3_total if o3_total > 0 else 0

    total_row = f"│ {'TOTAL':<{W_STAGE}} "
    for flag in flags:
        tot = totals[flag]
        total_row += f"│ {tot:>{W_CYCLES},} │ {'100.0%':^{W_PCT}} "
    total_row += f"│ {total_speedup:.1f}×  {'★':^6} │"
    print(total_row)
    print(bot_line)

    # ── Amdahl's Law section ──
    print()
    print("  Amdahl's Law Analysis:")
    print("  ─────────────────────────────────────────────────────")

    o3_total = totals.get("-O3", 1)
    for stage in STAGES:
        cyc = all_results["-O3"].get(stage, 0)
        pct = 100.0 * cyc / o3_total
        o0  = all_results["-O0"].get(stage, 0)
        o3  = all_results["-O3"].get(stage, 0)
        sp  = o0 / o3 if o3 > 0 else 0

        if pct >= 30:
            verdict = "◄ HIGH PRIORITY — vectorize with RVV!"
        elif pct >= 15:
            verdict = "◄ MEDIUM — worth vectorizing"
        else:
            verdict = "  LOW — skip, small gain"

        print(f"  {stage:<12} {pct:5.1f}% of -O3 time  {verdict}")

    print()
    print("  Tip: Focus RVV effort on stages with >30% of total time.")
    print("       Optimizing small stages gives almost no total speedup.")
    print()

# ── Main ────────────────────────────────────────────────────────
if __name__ == "__main__":
    if len(sys.argv) < 4:
        print("Usage: python3 scripts/sweep_table.py <input.raw> <W> <H>")
        sys.exit(1)

    raw_input = sys.argv[1]
    w, h      = sys.argv[2], sys.argv[3]

    print()
    print(f"  Running all optimization levels on {raw_input} ({w}×{h})...")
    print(f"  (This will take a minute — each binary runs 100 iterations)")
    print()

    all_results = {}

    for flag, binary in BINARIES.items():
        print(f"  [{flag}] Running {binary} ...", end="", flush=True)
        output = run_binary(binary, raw_input, w, h)

        if output:
            cycles = parse_cycles(output)
            all_results[flag] = cycles
            total = sum(cycles.values())
            print(f" done  (total: {total:,} cycles)")
        else:
            print(f" FAILED")
            all_results[flag] = {s: 0 for s in STAGES}

    print_comparison_table(all_results)
