# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import matplotlib.pyplot as plt

fig, ax = plt.subplots(figsize=(6.0, 4.0))
ax.plot([0, 1, 2], [0, 1, 4])
fig.text(0.5, 0.02, 'a note under the axes: $\\sigma=\\sqrt{\\frac{1}{N}\\sum_i x_i^2}$', ha='center', color='C0')
fig.text(0.02, 0.5, 'up the side', rotation='vertical', va='center', fontsize=14)
fig.text(x=0.98, y=0.98, s='corner', ha='right', va='top', alpha=0.5, zorder=10)
fig.text(0.5, 0.5, 'over the axes', ha='center', fontsize=30, color=[0.5, 0.5, 0.5, 0.3])
fig.text(1.2, 0.5, 'off the figure')
fig.text(0.3, 0.9, 'hidden', visible=False)
fig.suptitle('with a suptitle')

if __name__ == "__main__":
    plt.show()
