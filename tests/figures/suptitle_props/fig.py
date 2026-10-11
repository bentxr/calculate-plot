# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import matplotlib.pyplot as plt

fig, axs = plt.subplots(1, 2, figsize=(7.0, 3.5))
fig.suptitle('A figure title in $\\mathcal{STIX}$, turned a little', color='C3', style='italic', family='serif', rotation=3, alpha=0.8, math_fontfamily='stix', size=15, weight='light', ha='left', x=0.1)

if __name__ == "__main__":
    plt.show()
