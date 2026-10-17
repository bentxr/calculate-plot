# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import datetime
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(2, 1, sharex=True, figsize=(6.0, 5.0))
axs[0].plot([datetime.datetime(2019, 12, 20, 0, 0), datetime.datetime(2019, 12, 24, 0, 0), datetime.datetime(2019, 12, 28, 0, 0), datetime.datetime(2020, 1, 1, 0, 0), datetime.datetime(2020, 1, 5, 0, 0), datetime.datetime(2020, 1, 9, 0, 0), datetime.datetime(2020, 1, 13, 0, 0), datetime.datetime(2020, 1, 17, 0, 0), datetime.datetime(2020, 1, 21, 0, 0), datetime.datetime(2020, 1, 25, 0, 0), datetime.datetime(2020, 1, 29, 0, 0), datetime.datetime(2020, 2, 2, 0, 0), datetime.datetime(2020, 2, 6, 0, 0), datetime.datetime(2020, 2, 10, 0, 0), datetime.datetime(2020, 2, 14, 0, 0)], [0.0, 0.744218, 1.18545, 1.163209, 0.734988, 0.149217, -0.271576, -0.282453, 0.168733, 0.916814, 1.656987, 2.088168, 2.054599, 1.619098, 1.033521])
axs[1].plot([18250, 18255, 18260, 18265, 18270, 18275, 18280, 18285, 18290, 18295, 18300, 18305], [0.0, 2.032653, 3.156349, 2.889628, 1.404964, -0.55235, -2.014727, -2.247358, -1.0938, 0.950442, 2.97096, 4.064505], color='C3')
axs[1].set_xlabel('date')
fig.tight_layout()

if __name__ == "__main__":
    plt.show()
