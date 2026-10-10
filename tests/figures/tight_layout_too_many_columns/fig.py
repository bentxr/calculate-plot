# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import matplotlib.pyplot as plt

fig, axs = plt.subplots(1, 4, figsize=(1.8, 3.0))
axs[0].set_ylabel('first')
axs[1].set_ylabel('second')
axs[2].set_ylabel('third')
axs[3].set_ylabel('fourth')
fig.tight_layout()

if __name__ == "__main__":
    plt.show()
