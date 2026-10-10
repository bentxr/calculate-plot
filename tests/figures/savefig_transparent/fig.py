# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import matplotlib.pyplot as plt

plt.rcParams.update({
    'savefig.transparent': True,
    'savefig.facecolor': 'green',
})
fig, axs = plt.subplots(1, 2, figsize=(4.0, 2.0))
axs[0].plot([0, 1], [0, 1])
axs[1].axhspan(0.2, 0.6)

if __name__ == "__main__":
    plt.show()
