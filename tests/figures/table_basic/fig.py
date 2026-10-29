# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(1, 2, figsize=(10.0, 4.5))
axs[0].plot([0, 1, 2], [1, 3, 2])
axs[0].table(cellText=[['1', '22', '333'], ['a long cell text', 'b', 'c']], rowLabels=['first', 'second'], colLabels=['A', 'B', 'C'], loc='bottom')
axs[0].set_title('table below')
axs[1].table(cellText=[['x', 'y'], ['1.5', '2.5'], ['3', '4']], cellColours=[['C1', 'w'], ['w', '0.8'], ['#a0d0ff', 'w']], colWidths=[0.3, 0.2], cellLoc='center', colLabels=['one', 'two'], colColours=['y', 'y'], loc='upper left', edges='horizontal')
axs[1].table(cellText=[['p', 'q', 'r']], rowLabels=['row'], rowColours=['0.9'], bbox=[0.3, 0.1, 0.6, 0.2], fontsize=14)

if __name__ == "__main__":
    plt.show()
