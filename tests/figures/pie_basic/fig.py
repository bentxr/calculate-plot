# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(2, 2, figsize=(7.2, 6))
axs[0, 0].pie([15, 30, 45, 10], labels=['Frogs', 'Hogs', 'Dogs', 'Logs'], autopct='%1.1f%%', explode=[0, 0.1, 0, 0], shadow=True, startangle=90)
axs[0, 1].pie([3, 2, 1], colors=['C3', 'gold', [0.2, 0.4, 0.6]], counterclock=False, radius=0.8, labeldistance=None, hatch=['/', '.', ''], frame=True, center=(0.5, 0.2))
axs[1, 0].pie([0.2, 0.3], normalize=False, rotatelabels=True, labels=['a', 'b'], pctdistance=0.8, autopct='%.0f')
axs[1, 1].pie([1, 1, 1, 1], labels=['w', 'x', 'y', 'z'])
axs[1, 1].legend()

if __name__ == "__main__":
    plt.show()
