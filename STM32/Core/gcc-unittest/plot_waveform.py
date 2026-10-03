"""Plot waveform samples exported by test_main.c."""

from __future__ import annotations

import argparse
import csv
from pathlib import Path

import matplotlib.pyplot as plt


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


def main() -> None:
    parser = argparse.ArgumentParser(description="Plot a generated waveform CSV")
    parser.add_argument(
        "csv_path",
        nargs="?",
        type=Path,
        default=Path(__file__).with_name("waveform.csv"),
        help="CSV file to plot (default: waveform.csv beside this script)",
    )
    args = parser.parse_args()

    sample_indices, outputs = read_waveform(args.csv_path)
    if not outputs:
        raise ValueError(f"No samples found in {args.csv_path}")

    plt.figure(figsize=(12, 5))
    plt.plot(sample_indices, outputs, linewidth=0.8)
    plt.title(f"Waveform: {args.csv_path.name}")
    plt.xlabel("Sample index")
    plt.ylabel("Output")
    plt.grid(True, alpha=0.3)
    plt.tight_layout()
    plt.show()


if __name__ == "__main__":
    main()