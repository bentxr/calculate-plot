# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import matplotlib.pyplot as plt

plt.rcParams.update({
    'savefig.bbox': 'tight',
})
fig, ax = plt.subplots(figsize=(4.0, 3.0))
ax.plot([0, 1, 2], [0, 1000, 4000])
ax.set_xlabel('time (s)')
ax.set_ylabel('counts')
ax.set_title('A title')
fig.suptitle('Above everything', y=1.05)

if __name__ == "__main__":
    plt.show()
