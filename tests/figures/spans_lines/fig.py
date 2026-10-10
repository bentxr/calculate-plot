# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import matplotlib.pyplot as plt

fig, ax = plt.subplots(figsize=(4.0, 3.0))
ax.plot([0, 1, 2], [0, 1, 4])
ax.axhline(2, color='r', ls='--')
ax.axvline(x=1.5, ymin=0.2, ymax=0.8)
ax.axhspan(0.5, 1.5, alpha=0.3)
ax.axvspan(0.2, 0.6, color='C2', alpha=0.2)
ax.vlines([0.5, 1.0], 0, [1, 2])
ax.hlines(3.5, 0, 2, colors='k', linestyles='dotted')

if __name__ == "__main__":
    plt.show()
