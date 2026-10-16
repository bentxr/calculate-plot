# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(1, 2, figsize=(9, 5))
axs[0].set(xlim=(0, 10), ylim=(0, 12))
axs[1].set(xlim=(0, 10), ylim=(0, 10))
axs[0].text(1.0, 11.0, 'None', bbox={'fc': 'C0', 'alpha': 0.5}, fontsize=7, rotation=0)
axs[0].text(4.2, 11.0, 'square', bbox={'boxstyle': 'square', 'fc': 'w', 'ec': 'C1'}, fontsize=8, rotation=0)
axs[0].text(7.4, 11.0, 'circle', bbox={'boxstyle': 'circle', 'fc': 'w', 'ec': 'C2'}, fontsize=9, rotation=0)
axs[0].text(1.0, 8.8, 'ellipse', bbox={'boxstyle': 'ellipse', 'fc': 'w', 'ec': 'C3'}, fontsize=10, rotation=0)
axs[0].text(4.2, 8.8, 'larrow', bbox={'boxstyle': 'larrow', 'fc': 'w', 'ec': 'C4', 'pad': 0.6}, fontsize=7, rotation=0)
axs[0].text(7.4, 8.8, 'rarrow', bbox={'boxstyle': 'rarrow', 'fc': 'w', 'ec': 'C5'}, fontsize=8, rotation=15)
axs[0].text(1.0, 6.6, 'darrow', bbox={'boxstyle': 'darrow', 'fc': 'w', 'ec': 'C6'}, fontsize=9, rotation=0)
axs[0].text(4.2, 6.6, 'round', bbox={'boxstyle': 'round', 'fc': 'w', 'ec': 'C7', 'lw': 2, 'ls': '--'}, fontsize=10, rotation=0)
axs[0].text(7.4, 6.6, 'round4', bbox={'boxstyle': 'round4', 'fc': 'w', 'ec': 'C8'}, fontsize=7, rotation=0)
axs[0].text(1.0, 4.3999999999999995, 'sawtooth', bbox={'boxstyle': 'sawtooth', 'fc': 'w', 'ec': 'C9'}, fontsize=8, rotation=0)
axs[0].text(4.2, 4.3999999999999995, 'roundtooth', bbox={'boxstyle': 'roundtooth', 'fc': 'w', 'ec': 'C0'}, fontsize=9, rotation=0)
axs[0].text(7.4, 4.3999999999999995, 'round,pad=0.5,rounding_size=0.8', bbox={'boxstyle': 'round,pad=0.5,rounding_size=0.8', 'fc': 'w', 'ec': 'C1'}, fontsize=10, rotation=0)
axs[0].text(1.0, 2.1999999999999993, 'rarrow,head_width=0.8,head_angle=150', bbox={'boxstyle': 'rarrow,head_width=0.8,head_angle=150', 'fc': 'w', 'ec': 'C2'}, fontsize=7, rotation=0)
axs[0].text(4.2, 2.1999999999999993, 'darrow,head_angle=300', bbox={'boxstyle': 'darrow,head_angle=300', 'fc': 'w', 'ec': 'C3'}, fontsize=8, rotation=0)
axs[0].text(7.4, 2.1999999999999993, 'sawtooth,tooth_size=0.2', bbox={'boxstyle': 'sawtooth,tooth_size=0.2', 'fc': 'w', 'ec': 'C4'}, fontsize=9, rotation=0)
axs[1].text(1, 9, 'background', backgroundcolor='0.8')
axs[1].text(5, 9, 'pad 2', bbox={'pad': 2, 'fc': 'y'}, fontsize=12, ha='center')
axs[1].annotate('boxed', (2, 2), (5, 5), bbox={'boxstyle': 'round', 'fc': '0.9'}, arrowprops={'arrowstyle': '->'})
axs[1].annotate('circle', (8, 2), (7, 6), bbox={'boxstyle': 'circle', 'fc': 'C1', 'alpha': 0.4}, arrowprops={'arrowstyle': '-|>', 'connectionstyle': 'arc3,rad=0.4'}, ha='center', va='center')
axs[1].annotate('simple', (1, 6), (3, 7.5), bbox={'boxstyle': 'larrow,pad=0.3', 'fc': 'C2', 'ec': 'k', 'lw': 1.5}, arrowprops={'facecolor': 'black', 'shrink': 0.05})
axs[1].text(8, 8.5, 'zorder', bbox={'boxstyle': 'round4', 'fc': 'w', 'hatch': 'xx'}, zorder=5)

if __name__ == "__main__":
    plt.show()
