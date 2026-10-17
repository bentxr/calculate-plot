# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

plt.rcParams.update({
    'font.size': 0.5,
    'axes.labelsize': 0.3,
    'axes.titlesize': 0.75,
    'xtick.labelsize': 0.75,
    'ytick.labelsize': '0.2',
    'legend.fontsize': 0.2,
    'legend.title_fontsize': 0.9,
    'figure.titlesize': 0.4,
})
fig, axs = plt.subplots(1, 2, figsize=(4.0, 2.0))
axs[0].plot([0, 1, 2], [1234500.0, 1234502.5, 1234501.0], label='a line')
axs[0].set_title('A title')
axs[0].set_xlabel('x label')
axs[0].set_ylabel('y label')
axs[0].legend(title='Legend')
axs[0].text(1, 1234501.5, 'a note')
axs[1].plot([1, 10, 100], [1, 1000, 10], label='another')
axs[1].set_xscale('log')
axs[1].legend(fontsize=0.5, title='T', title_fontsize=0.1)
axs[1].set_title('sized', fontsize=0.9)
fig.suptitle('The figure')
fig.tight_layout()

if __name__ == "__main__":
    plt.show()
