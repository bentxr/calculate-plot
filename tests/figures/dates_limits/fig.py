# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import datetime
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(1, 3, figsize=(12.0, 3.5))
axs[0].set_xlim(datetime.datetime(2000, 2, 1, 0, 0), datetime.datetime(2000, 2, 3, 12, 0))
axs[1].plot([datetime.datetime(2010, 1, 1, 0, 0), datetime.datetime(2010, 2, 20, 0, 0), datetime.datetime(2010, 4, 11, 0, 0), datetime.datetime(2010, 5, 31, 0, 0), datetime.datetime(2010, 7, 20, 0, 0), datetime.datetime(2010, 9, 8, 0, 0), datetime.datetime(2010, 10, 28, 0, 0), datetime.datetime(2010, 12, 17, 0, 0), datetime.datetime(2011, 2, 5, 0, 0), datetime.datetime(2011, 3, 27, 0, 0), datetime.datetime(2011, 5, 16, 0, 0), datetime.datetime(2011, 7, 5, 0, 0)], [0.0, 0.744218, 1.18545, 1.163209, 0.734988, 0.149217, -0.271576, -0.282453, 0.168733, 0.916814, 1.656987, 2.088168])
axs[1].set_xlim((datetime.datetime(2010, 3, 1, 0, 0), datetime.datetime(2011, 1, 1, 0, 0)))
axs[2].plot([0.0, 0.744218, 1.18545, 1.163209, 0.734988, 0.149217, -0.271576, -0.282453, 0.168733, 0.916814], [datetime.datetime(1999, 12, 31, 22, 0), datetime.datetime(1999, 12, 31, 22, 17), datetime.datetime(1999, 12, 31, 22, 34), datetime.datetime(1999, 12, 31, 22, 51), datetime.datetime(1999, 12, 31, 23, 8), datetime.datetime(1999, 12, 31, 23, 25), datetime.datetime(1999, 12, 31, 23, 42), datetime.datetime(1999, 12, 31, 23, 59), datetime.datetime(2000, 1, 1, 0, 16), datetime.datetime(2000, 1, 1, 0, 33)])
axs[2].set_ylim(bottom=datetime.datetime(1999, 12, 31, 21, 0), top=None)
fig.tight_layout()

if __name__ == "__main__":
    plt.show()
