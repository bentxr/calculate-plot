# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import matplotlib.pyplot as plt

fig, ax = plt.subplots(figsize=(4.0, 3.0), dpi=120)
ax.set_title('Pulled in', pad=-20)
ax.set_title('right side', loc='right')
ax.set_xlabel('x', loc='right', labelpad=10)
ax.set_ylabel('y (m/s)', loc='top')
ax.set_xlim(5, -5)
ax.set_ylim(top=2.5)

if __name__ == "__main__":
    plt.show()
