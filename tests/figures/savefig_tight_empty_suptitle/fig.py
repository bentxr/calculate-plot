# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import matplotlib.pyplot as plt

plt.rcParams.update({
    'savefig.bbox': 'tight',
})
fig, ax = plt.subplots(figsize=(3.0, 2.0))
ax.plot([0, 1], [0, 1])
fig.suptitle('', y=1.4, x=1.3)

if __name__ == "__main__":
    plt.show()
