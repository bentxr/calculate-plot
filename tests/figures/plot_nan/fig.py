# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import matplotlib.pyplot as plt

fig, ax = plt.subplots(figsize=(4.0, 3.0))
ax.plot([0, 1, 2, 3, 4, 5], [1, 2, float('nan'), 3, float('inf'), 2.5], marker='o')
ax.plot([float('nan'), 6, 7], [0, -1, float('-inf')], marker='s')

if __name__ == "__main__":
    plt.show()
