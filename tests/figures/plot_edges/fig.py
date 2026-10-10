# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import matplotlib.pyplot as plt

fig, ax = plt.subplots(figsize=(4.0, 3.0))
ax.plot([1, 2, 3], [5, 5, 5], '')
ax.plot([2], [5], '1', ms=3)
ax.plot([1.5, 2.5], [5.01, 4.99], '^', [1, 3], [4.995, 5.005], 'k-', markersize=3)
ax.plot([], [])

if __name__ == "__main__":
    plt.show()
