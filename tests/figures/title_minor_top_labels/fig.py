# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import matplotlib.pyplot as plt

plt.rcParams.update({
    'xtick.labeltop': True,
    'xtick.top': True,
    'xtick.major.top': False,
})
fig, ax = plt.subplots(figsize=(4.0, 3.0))
ax.plot([2, 3, 6], [0, 3, 1])
ax.set_xscale('log')
ax.set_title('Minor labels on top only')

if __name__ == "__main__":
    plt.show()
