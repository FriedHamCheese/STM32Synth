"""Plot waveform samples exported by test_main.c."""

from __future__ import annotations

import argparse
import csv
from pathlib import Path


def read_waveform(csv_path: Path) -> tuple[list[int], list[int]]:
    sample_indices: list[int] = []
    outputs: list[int] = []

    with csv_path.open(newline="") as csv_file:
        reader = csv.DictReader(csv_file)
        if reader.fieldnames != ["sample_index", "output"]:
            raise ValueError(
                "Expected CSV columns: sample_index,output"
            )

        for row in reader:
            sample_indices.append(int(row["sample_index"]))
            outputs.append(float(row["output"]))

    return sample_indices, outputs


def plot_ascii(outputs: list[float], width: int = 100, height: int = 20) -> None:
    """Print min/max envelope bars; no matplotlib needed."""
    if len(outputs) > width:
        bucket = len(outputs) / width
        lows: list[float] = []
        highs: list[float] = []
        for col in range(width):
            start = int(col * bucket)
            end = max(start + 1, int((col + 1) * bucket))
            chunk = outputs[start:end]
            lows.append(min(chunk))
            highs.append(max(chunk))
    else:
        lows = list(outputs)
        highs = list(outputs)

    low = min(lows)
    high = max(highs)
    if high == low:
        high = low + 1.0

    for row in range(height):
        level = high - (high - low) * row / (height - 1)
        bars = "".join(
            "#" if lows[col] <= level <= highs[col] else " "
            for col in range(len(lows))
        )
        print(f"{level:7.1f} |{bars}|")
    print(f"        +{'-' * len(lows)}+")
    print(f"        0..{len(outputs) - 1} samples, min={low:.1f} max={high:.1f}")


def main() -> None:
    parser = argparse.ArgumentParser(description="Plot a generated waveform CSV")
    parser.add_argument(
        "csv_path",
        nargs="?",
        type=Path,
        default=Path(__file__).with_name("waveform.csv"),
        help="CSV file to plot (default: waveform.csv beside this script)",
    )
    parser.add_argument(
        "--save",
        type=Path,
        default=None,
        help="Save the plot to this path instead of opening a window",
    )
    parser.add_argument(
        "--ascii",
        action="store_true",
        help="Print a terminal envelope plot instead of using matplotlib",
    )
    parser.add_argument("--width", type=int, default=100)
    parser.add_argument("--height", type=int, default=20)
    parser.add_argument(
        "--limit",
        type=int,
        default=0,
        help="Only plot the first N samples (0 = all)",
    )
    args = parser.parse_args()

    sample_indices, outputs = read_waveform(args.csv_path)
    if not outputs:
        raise ValueError(f"No samples found in {args.csv_path}")
    if args.limit > 0:
        sample_indices = sample_indices[:args.limit]
        outputs = outputs[:args.limit]

    if args.ascii:
        plot_ascii(outputs, args.width, args.height)
        return

    import matplotlib.pyplot as plt

    plt.figure(figsize=(12, 5))
    plt.plot(sample_indices, outputs, linewidth=0.8)
    plt.title(f"Waveform: {args.csv_path.name}")
    plt.xlabel("Sample index")
    plt.ylabel("Output")
    plt.grid(True, alpha=0.3)
    plt.tight_layout()
    if args.save:
        plt.savefig(args.save, dpi=150)
        print(f"Saved {args.save}")
    else:
        plt.show()


if __name__ == "__main__":
    main()