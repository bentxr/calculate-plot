# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import matplotlib.pyplot as plt

plt.rcParams.update({
    'savefig.bbox': 'tight',
    'savefig.pad_inches': 0,
    'savefig.dpi': 72,
    'savefig.facecolor': '0.9',
})
fig, ax = plt.subplots(figsize=(3.0, 2.0))
ax.plot([0, 1], [0, 1], label='line')
ax.legend(loc='upper left')
ax.set_title('Right', loc='right')

if __name__ == "__main__":
    plt.show()
