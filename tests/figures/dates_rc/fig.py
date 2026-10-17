# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import datetime
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

plt.rcParams.update({
    'date.autoformatter.day': '%d/%m',
    'date.autoformatter.hour': '%Hh',
    'date.autoformatter.minute': '%H:%M',
    'date.interval_multiples': False,
})
fig, axs = plt.subplots(1, 2, figsize=(9.0, 3.5))
axs[0].plot([datetime.datetime(2022, 7, 9, 13, 30)], [1], marker='x')
axs[1].plot([datetime.datetime(2022, 7, 9, 13, 7), datetime.datetime(2022, 7, 9, 13, 20), datetime.datetime(2022, 7, 9, 13, 33), datetime.datetime(2022, 7, 9, 13, 46), datetime.datetime(2022, 7, 9, 13, 59), datetime.datetime(2022, 7, 9, 14, 12), datetime.datetime(2022, 7, 9, 14, 25), datetime.datetime(2022, 7, 9, 14, 38), datetime.datetime(2022, 7, 9, 14, 51), datetime.datetime(2022, 7, 9, 15, 4), datetime.datetime(2022, 7, 9, 15, 17), datetime.datetime(2022, 7, 9, 15, 30)], [0.0, 0.744218, 1.18545, 1.163209, 0.734988, 0.149217, -0.271576, -0.282453, 0.168733, 0.916814, 1.656987, 2.088168])
fig.tight_layout()

if __name__ == "__main__":
    plt.show()
