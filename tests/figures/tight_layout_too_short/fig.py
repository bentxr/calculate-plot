# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import matplotlib.pyplot as plt

fig, ax = plt.subplots(figsize=(3.0, 0.6))
ax.set_xlabel('an x label')
fig.tight_layout()

if __name__ == "__main__":
    plt.show()
