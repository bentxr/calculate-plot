# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

plt.rcParams.update({
    'font.size': 0.6,
    'axes.labelsize': 'xx-large',
    'axes.titlesize': 'large',
    'xtick.labelsize': 'x-large',
    'ytick.labelsize': 'small',
    'legend.fontsize': 'xx-large',
    'figure.titlesize': 'xx-large',
    'savefig.dpi': 300,
})
fig, axs = plt.subplots(1, 2, figsize=(4.0, 2.0))
axs[0].plot([0, 1, 2], [1234500.0, 1234502.5, 1234501.0], label='a line')
axs[0].set_title('A title')
axs[0].set_xlabel('x label')
axs[0].set_ylabel('y label')
axs[0].legend(title='Legend')
axs[0].text(1, 1234501.5, 'a note', fontsize='xx-large')
axs[1].plot([1, 10, 100], [1, 1000, 10], label='another')
axs[1].set_yscale('log')
axs[1].legend(fontsize='x-large', title='T', title_fontsize='larger')
axs[1].set_title('sized', fontsize='smaller')
fig.suptitle('The figure')
fig.tight_layout()

if __name__ == "__main__":
    plt.show()
