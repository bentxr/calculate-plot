# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(1, 2, figsize=(6.0, 3.0))
axs[0].matshow([[0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0], [0.0, 0.8415, 1.6829, 2.5244, 3.3659, 4.2074, 5.0488, 5.8903, 6.7318, 7.5732, 8.4147, 9.2562], [0.0, 0.9093, 1.8186, 2.7279, 3.6372, 4.5465, 5.4558, 6.3651, 7.2744, 8.1837, 9.093, 10.0023], [0.0, 0.1411, 0.2822, 0.4234, 0.5645, 0.7056, 0.8467, 0.9878, 1.129, 1.2701, 1.4112, 1.5523], [-0.0, -0.7568, -1.5136, -2.2704, -3.0272, -3.784, -4.5408, -5.2976, -6.0544, -6.8112, -7.568, -8.3248], [-0.0, -0.9589, -1.9178, -2.8768, -3.8357, -4.7946, -5.7535, -6.7125, -7.6714, -8.6303, -9.5892, -10.5482], [-0.0, -0.2794, -0.5588, -0.8382, -1.1177, -1.3971, -1.6765, -1.9559, -2.2353, -2.5147, -2.7942, -3.0736]])
axs[0].set_title('matrix')
axs[1].matshow([[0, 1, 2], [3, 4, 5], [6, 7, 8]], cmap='Blues', alpha=0.8)

if __name__ == "__main__":
    plt.show()
