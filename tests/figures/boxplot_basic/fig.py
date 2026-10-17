# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(2, 2, figsize=(7.2, 5.4))
axs[0, 0].boxplot([[1, 2, 3, 4, 5, 6, 7, 8, 20], [2.5, 3, 3.2, 4, 5, 6.1, 9, -4]])
axs[0, 1].boxplot([[1, 2, 3, 4, 5, 6, 7, 8, 20], [2.5, 3, 3.2, 4, 5, 6.1, 9, -4]], notch=True, patch_artist=True, showmeans=True, tick_labels=['a', 'b'], label=['A', 'B'])
axs[0, 1].legend()
axs[1, 0].boxplot([0.5, 1.2, 1.9, 2.2, 2.4, 3.1, 3.3, 3.8, 4.4, 5.0, 9.5], orientation='horizontal', whis=[5, 95], sym='r+', meanline=True, showmeans=True, widths=0.3, positions=[2])
axs[1, 1].boxplot([[1, 1, 2, 3, 5, 8, 13], [2, 2, 2, 2]], showcaps=False, whis=0.5, positions=[1, 2.5], autorange=True, capwidths=0.2, label='first')
axs[1, 1].legend()

if __name__ == "__main__":
    plt.show()
