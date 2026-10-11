# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import matplotlib.pyplot as plt

plt.rcParams.update({
    'font.family': 'serif, monospace',
    'font.serif': 'no such serif,DejaVu Sans Mono ,',
    'font.monospace': ['', 'DejaVu Serif'],
})
fig, ax = plt.subplots(figsize=(5.0, 3.5))
ax.plot([0, 1, 2], [0, 1, 4], label='in the first family')
ax.set_title('The serif list was given as one string')
ax.set_xlabel('monospace is a serif here', family='monospace')
ax.legend()

if __name__ == "__main__":
    plt.show()
