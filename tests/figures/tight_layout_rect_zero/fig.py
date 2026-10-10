# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import matplotlib.pyplot as plt

plt.rcParams.update({
    'font.size': 14,
})
fig, axs = plt.subplots(1, 2, figsize=(5.0, 3.0))
axs[0].set_ylabel('y')
axs[1].set_xlabel('x')
fig.tight_layout(rect=[0, 0, 1, 1])

if __name__ == "__main__":
    plt.show()
