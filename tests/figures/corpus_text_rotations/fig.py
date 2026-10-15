# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

plt.rcParams.update({
    'backend_fallback': False,
})
fig1 = plt.figure(figsize=(3.2, 2.4), dpi=100)
fig1.text(0.1, 0.5, 'Angle 0', rotation=0, fontsize=8)
fig1.text(0.25, 0.5, 'Angle 15', rotation=15, fontsize=9)
fig1.text(0.4, 0.5, 'Angle 45', rotation=45, fontsize=10)
fig1.text(0.5499999999999999, 0.5, 'Angle 90', rotation=90, fontsize=11)
fig1.text(0.7, 0.5, 'Angle 180', rotation=180, fontsize=12)
fig1.text(0.85, 0.5, 'Angle 270', rotation=270, fontsize=13)
fig1.text(0.5, 0.9, 'Centered Title', ha='center', va='top', fontsize=14, fontweight='bold')
fig1.text(0.02, 0.05, 'italic small', style='italic', fontsize=7)
fig1.text(0.6, 0.05, 'Mono 123', family='monospace', fontsize=9)

if __name__ == "__main__":
    plt.show()
