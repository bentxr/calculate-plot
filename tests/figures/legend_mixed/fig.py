# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import matplotlib.pyplot as plt

fig, ax = plt.subplots(figsize=(4.0, 3.0))
ax.plot([0, 1, 2], [1, 2, 1.5], label='line', ls='--', marker='s')
ax.scatter([0.5, 1.5], [1, 1.8], label='points', c='C3')
ax.fill_between([0, 1, 2], [0.2, 0.6, 0.4], label='band', alpha=0.4)
ax.legend(loc='lower right', title='kinds')

if __name__ == "__main__":
    plt.show()
