# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import matplotlib.pyplot as plt

fig, ax = plt.subplots(figsize=(6.0, 4.5))
ax.axvspan(0.0, 1.0, label='plain')
ax.axvspan(1.0, 2.0, fc='C1', ec='k', lw=2.5, ls='--', label='dashed edge')
ax.axvspan(2.0, 3.0, color='C2', alpha=0.4, label='half there')
ax.axhspan(0.1, 0.3, fill=False, ec='C3', lw=1.5, ls=':', label='no fill, dotted')
ax.axhspan(0.5, 0.6, fc='none', ec='C4', label='no face')
ax.axhspan(0.7, 0.8, fc='#33669980', ec='none', lw=0, label='RGBA face', zorder=3)
ax.axhspan(0.85, 0.9, label='_hidden from the legend')
ax.plot([0, 3], [0, 1], label='a line among them')
ax.legend(loc='center left', ncols=2, fontsize=8)

if __name__ == "__main__":
    plt.show()
