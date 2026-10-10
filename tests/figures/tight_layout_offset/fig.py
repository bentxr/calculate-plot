# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import matplotlib.pyplot as plt

fig, ax = plt.subplots(figsize=(3.0, 2.5))
ax.plot([10000, 10001], [0, 1e-05])
fig.tight_layout()

if __name__ == "__main__":
    plt.show()
