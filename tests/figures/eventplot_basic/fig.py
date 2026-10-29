# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(1, 2, figsize=(9.0, 4.0))
axs[0].eventplot([[0.5, 1.2, 3.3, 2.1], [1, 2.5, 4], [0.2, 0.4, 3.9, 4.4, 2.2]])
axs[0].eventplot([1.5, 2.0, 3.0], lineoffsets=6, linelengths=0.5, colors='r', linewidths=2, linestyles='dashed', label='red')
axs[0].legend()
axs[1].eventplot([[1, 2, 3], [2, 4], [0.5, 3.5]], orientation='vertical', lineoffsets=[1, 3, 4], linelengths=[1, 0.5, 2], colors=['C1', 'k', '0.5'], alpha=0.7)
plt.eventplot([5, 6], lineoffsets=-1, colors=[[0.2, 0.4, 0.6]])

if __name__ == "__main__":
    plt.show()
