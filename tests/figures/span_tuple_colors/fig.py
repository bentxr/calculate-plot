# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import matplotlib.pyplot as plt

fig, ax = plt.subplots(figsize=(6.0, 4.0))
ax.axvspan(0.0, 1.0, color=[0.2, 0.6, 0.3], label='RGB list')
ax.axvspan(1.0, 2.0, fc=(0.8, 0.2, 0.2, 0.4), ec=[0.0, 0.0, 0.5], lw=2, label='RGBA tuple face')
ax.axhspan(0.2, 0.4, facecolor=[0.1, 0.1, 0.9, 0.9], alpha=0.3, label='alpha over an RGBA')
ax.axhspan(0.6, 0.8, edgecolor=(1.0, 0.5, 0.0, 0.5), fill=False, lw=3, label='edge only')
ax.legend(facecolor=[0.95, 0.95, 1.0], edgecolor=(0.0, 0.0, 0.5, 0.5), loc='lower right')

if __name__ == "__main__":
    plt.show()
