# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import matplotlib.pyplot as plt

plt.rcParams.update({
    'legend.handleheight': 1.4,
})
fig, ax = plt.subplots(figsize=(4.0, 3.0))
ax.plot([0, 1], [0, 1], label='one')
ax.plot([0, 1], [0, 2], label='two\nlines')
ax.plot([0, 1], [0, 3], label='three', marker='o')
ax.plot([0, 1], [0, 4], label='four')
ax.plot([0, 1], [0, 5], label='five')
ax.legend(ncols=2, loc='upper left')

if __name__ == "__main__":
    plt.show()
