# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(2, 2, figsize=(8, 6))
axs[0, 0].set(xlim=(0, 10), ylim=(0, 10))
axs[0, 1].set(xlim=(0, 10), ylim=(0, 10))
axs[1, 0].set(xlim=(0, 10), ylim=(0, 10))
axs[1, 1].set(xlim=(0, 10), ylim=(0, 10))
axs[0, 0].annotate('-', (1, 9.5), (6, 9.5), arrowprops={'arrowstyle': '-', 'color': 'C0'}, fontsize=7, va='center')
axs[0, 0].annotate('->', (1, 8.9), (6, 8.9), arrowprops={'arrowstyle': '->', 'color': 'C1'}, fontsize=7, va='center')
axs[0, 0].annotate('-[', (1, 8.3), (6, 8.3), arrowprops={'arrowstyle': '-[', 'color': 'C2'}, fontsize=7, va='center')
axs[0, 0].annotate('|-|', (1, 7.7), (6, 7.7), arrowprops={'arrowstyle': '|-|', 'color': 'C3'}, fontsize=7, va='center')
axs[0, 0].annotate('-|>', (1, 7.1), (6, 7.1), arrowprops={'arrowstyle': '-|>', 'color': 'C4'}, fontsize=7, va='center')
axs[0, 0].annotate('<-', (1, 6.5), (6, 6.5), arrowprops={'arrowstyle': '<-', 'color': 'C5'}, fontsize=7, va='center')
axs[0, 0].annotate('<->', (1, 5.9), (6, 5.9), arrowprops={'arrowstyle': '<->', 'color': 'C6'}, fontsize=7, va='center')
axs[0, 0].annotate('<|-', (1, 5.3), (6, 5.3), arrowprops={'arrowstyle': '<|-', 'color': 'C7'}, fontsize=7, va='center')
axs[0, 0].annotate('<|-|>', (1, 4.7), (6, 4.7), arrowprops={'arrowstyle': '<|-|>', 'color': 'C8'}, fontsize=7, va='center')
axs[0, 0].annotate(']-', (1, 4.1000000000000005), (6, 4.1000000000000005), arrowprops={'arrowstyle': ']-', 'color': 'C9'}, fontsize=7, va='center')
axs[0, 0].annotate(']-[', (1, 3.5), (6, 3.5), arrowprops={'arrowstyle': ']-[', 'color': 'C0'}, fontsize=7, va='center')
axs[0, 0].annotate(']->', (1, 2.9000000000000004), (6, 2.9000000000000004), arrowprops={'arrowstyle': ']->', 'color': 'C1'}, fontsize=7, va='center')
axs[0, 0].annotate('<-[', (1, 2.3000000000000007), (6, 2.3000000000000007), arrowprops={'arrowstyle': '<-[', 'color': 'C2'}, fontsize=7, va='center')
axs[0, 0].annotate('fancy', (1, 1.7000000000000002), (6, 1.7000000000000002), arrowprops={'arrowstyle': 'fancy', 'color': 'C3'}, fontsize=7, va='center')
axs[0, 0].annotate('simple', (1, 1.0999999999999996), (6, 1.0999999999999996), arrowprops={'arrowstyle': 'simple', 'color': 'C4'}, fontsize=7, va='center')
axs[0, 0].annotate('wedge', (1, 0.5), (6, 0.5), arrowprops={'arrowstyle': 'wedge', 'color': 'C5'}, fontsize=7, va='center')
axs[0, 1].annotate('c0', (1.0, 8), (2.2, 9.5), arrowprops={'arrowstyle': '->', 'connectionstyle': 'arc3,rad=0.3'}, fontsize=8)
axs[0, 1].annotate('c1', (3.3, 8), (4.5, 9.5), arrowprops={'arrowstyle': '->', 'connectionstyle': 'arc3,rad=-0.4'}, fontsize=8)
axs[0, 1].annotate('c2', (5.6, 8), (6.8, 9.5), arrowprops={'arrowstyle': '->', 'connectionstyle': 'angle3,angleA=90,angleB=0'}, fontsize=8)
axs[0, 1].annotate('c3', (7.8999999999999995, 8), (9.1, 9.5), arrowprops={'arrowstyle': '->', 'connectionstyle': 'angle,angleA=-90,angleB=180,rad=5'}, fontsize=8)
axs[0, 1].annotate('c4', (1.0, 3), (2.2, 4.5), arrowprops={'arrowstyle': '->', 'connectionstyle': 'arc,angleA=-90,angleB=0,armA=30,armB=30,rad=7'}, fontsize=8)
axs[0, 1].annotate('c5', (3.3, 3), (4.5, 4.5), arrowprops={'arrowstyle': '->', 'connectionstyle': 'bar,fraction=0.3'}, fontsize=8)
axs[0, 1].annotate('c6', (5.6, 3), (6.8, 4.5), arrowprops={'arrowstyle': '->', 'connectionstyle': 'bar,angle=180,fraction=-0.2'}, fontsize=8)
axs[0, 1].annotate('c7', (7.8999999999999995, 3), (9.1, 4.5), arrowprops={'arrowstyle': '->', 'connectionstyle': 'arc,angleA=0,armA=20,rad=0'}, fontsize=8)
axs[0, 1].annotate('', (2, 1), (5, 2), arrowprops={'arrowstyle': '<->', 'shrinkA': 0, 'shrinkB': 0})
axs[1, 0].plot([1, 3, 5, 7, 9], [2, 8, 3, 7, 4])
axs[1, 0].annotate('local max', (3, 8), (4, 9.3), arrowprops={'facecolor': 'black', 'shrink': 0.05})
axs[1, 0].annotate('plain', (5, 3), (1.5, 0.8), arrowprops={})
axs[1, 0].annotate('wide', (9, 4), (6.5, 1), arrowprops={'width': 1, 'headwidth': 6, 'headlength': 5, 'color': 'C3', 'alpha': 0.5}, ha='center')
axs[1, 0].annotate('pts', (7, 7), xytext=(-40, 25), textcoords='offset points', arrowprops={'arrowstyle': 'fancy', 'fc': '0.6', 'ec': 'none', 'connectionstyle': 'arc3,rad=0.3'})
axs[1, 1].annotate('args', (2, 2), (5, 5), arrowprops={'arrowstyle': '->,head_length=0.8,head_width=0.4', 'lw': 2, 'ls': '--', 'relpos': (0, 0)})
axs[1, 1].annotate('bracket', (8, 2), (6, 6), arrowprops={'arrowstyle': '-[, widthB=2.0, lengthB=1.0', 'mutation_scale': 15, 'shrinkB': 10})
axs[1, 1].annotate('simple', (2, 8), (5, 9), arrowprops={'arrowstyle': 'simple,tail_width=0.5,head_width=2', 'fc': 'C2', 'ec': 'k', 'hatch': '//', 'mutation_scale': 10})
axs[1, 1].annotate('wedge', (9, 9), (7, 7.5), arrowprops={'arrowstyle': 'wedge,tail_width=0.7', 'fc': 'C4', 'alpha': 0.4, 'connectionstyle': 'arc3,rad=-0.3'}, fontsize=14)
axs[1, 1].annotate('axes', (0.1, 0.5), xycoords='axes fraction', xytext=(0.3, 0.3), textcoords='axes fraction', arrowprops={'arrowstyle': '-|>', 'mutation_aspect': 2, 'patchA': None})

if __name__ == "__main__":
    plt.show()
