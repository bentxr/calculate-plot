# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import matplotlib.pyplot as plt

fig, axs = plt.subplots(1, 3, figsize=(7.0, 2.5))
axs[0].axhspan(0.2, 0.6)
axs[1].axvspan(-3, 12, ymin=0.25, ymax=0.75)
axs[2].axhspan(5, 50)
axs[2].axvspan(2, 3, alpha=0.5)
axs[2].set_yscale('log')

if __name__ == "__main__":
    plt.show()
