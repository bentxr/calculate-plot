# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import matplotlib.pyplot as plt

plt.rcParams.update({
    'savefig.bbox': 'tight',
})
fig, ax = plt.subplots(figsize=(2.0, 1.6))
ax.set_title('A title much wider than the small figure it sits on')
ax.set_xlabel('and an x label that is also wider than the Axes')
ax.set_ylabel('a y label taller than the Axes are')

if __name__ == "__main__":
    plt.show()
