# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import matplotlib.pyplot as plt

plt.rcParams.update({
    'savefig.bbox': 'tight',
    'savefig.pad_inches': 0.05,
})
fig, axs = plt.subplots(2, 1, figsize=(3.0, 3.0), dpi=80, sharex=True)
axs[0].plot([0, 1, 2], [3, 1, 2])
axs[1].plot([0, 1, 2], [0.001, 0.002, 0.004])
axs[1].set_xlabel('shared')
fig.tight_layout()

if __name__ == "__main__":
    plt.show()
