# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import matplotlib.pyplot as plt

fig, axs = plt.subplots(2, 3, figsize=(10.0, 6.0))
axs[0, 0].set_title('big red bold', fontsize=16, color='red', fontweight='bold')
axs[0, 1].set_title('italic serif, left', loc='left', style='italic', family='serif', alpha=0.5)
axs[0, 1].set_title('right, turned', loc='right', rotation=15, c='C2', size='small')
axs[0, 2].set_title('moved with x and y', x=0.3, y=0.8, ha='left', va='top')
axs[1, 0].set_title('size, then fontsize: the last wins', size=20, fontsize=9, color='C0', c='C1')
axs[1, 1].set_title('centre asked to end here', horizontalalignment='right', verticalalignment='center', pad=12)
axs[1, 1].set_title('$x$ unparsed, rough', loc='left', parse_math=None, antialiased=None, y=0.5)
axs[1, 0].set_xlabel('$y$ unparsed, rough', parse_math=None, antialiased=None)
axs[1, 2].set_title('$\\frac{a}{b}$ in STIX, two\nlines to the right', math_fontfamily='stix', ma='right', linespacing=1.6, zorder=0.5, fontdict=None)

if __name__ == "__main__":
    plt.show()
