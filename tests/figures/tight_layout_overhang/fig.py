# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import matplotlib.pyplot as plt

fig, axs = plt.subplots(1, 2, figsize=(4.0, 2.5))
axs[0].plot([0, 1], [0, 1], label='a rather long legend label')
axs[0].legend(loc='upper left')
axs[1].set_title('A very long title for a narrow Axes')
axs[1].set_ylabel('a very long y label, much longer than the short Axes is tall')
fig.tight_layout()

if __name__ == "__main__":
    plt.show()
