# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, ax = plt.subplots(figsize=(6.0, 4.0))
ax.plot([0, 1, 2], [0, 1, 4])
ax.text(0.1, 3.5, 'plain first: 中 ∑ ℏ ⨁ ䷀', fontsize=14)
ax.text(0.1, 2.5, '$then\\ math: 中 ∑ ℏ ⨁ ䷀ x$', fontsize=14)
ax.text(0.1, 1.5, 'and serif: 中 ⨁ ䷀', fontsize=14, family='serif')
ax.text(0.1, 0.5, '$\\mathrm{中 ⨁ ䷀}\\mathit{中 ⨁ ䷀}\\mathbf{中}$', fontsize=14)
ax.set_title('$中 ∑ ℏ ⨁$ in the title, drawn last')
ax.set_xlabel('plain label 文')

if __name__ == "__main__":
    plt.show()
