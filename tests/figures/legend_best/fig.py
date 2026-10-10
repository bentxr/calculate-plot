# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import matplotlib.pyplot as plt

fig, ax = plt.subplots(figsize=(4.0, 3.0))
ax.plot([0, 1, 2, 3, 4], [4, 3, 2, 3, 4], label='valley')
ax.plot([0, 1, 2, 3, 4], [0, 0.5, 0.2, 0.4, 0.1], label='floor', marker='o')
ax.legend()

if __name__ == "__main__":
    plt.show()
