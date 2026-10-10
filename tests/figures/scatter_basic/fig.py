# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import matplotlib.pyplot as plt

fig, ax = plt.subplots(figsize=(4.0, 3.0))
ax.scatter([0, 1, 2, 3, 4], [2, 1, 4, 3, 5])
ax.scatter([0.5, 1.5, 2.5], [4, 2, 1], s=[20, 80, 200], c='C3', marker='s', alpha=0.7)

if __name__ == "__main__":
    plt.show()
