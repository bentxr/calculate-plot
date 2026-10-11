# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import matplotlib.pyplot as plt

fig, ax = plt.subplots(figsize=(6.0, 4.5))
ax.plot([0, 1, 2], [0, 1, 4], label='$\\hat{x}_i=\\frac{1}{2}$')
ax.plot([0, 1, 2], [4, 1, 0], label='$\\overline{AB}=\\left(\\frac{1}{2}\\right)$')
ax.set_title('$\\frac{a+b}{c}$ and $\\sqrt{x^2+1}$')
ax.set_xlabel('$\\sum_{i=0}^{n}\\frac{1}{i^2}$')
ax.set_ylabel('$\\sqrt[3]{\\frac{x}{y}}-\\underline{x}+\\binom{n}{k}$')
fig.suptitle('$\\genfrac{[}{]}{2}{0}{a}{b}$ over all')
ax.legend()

if __name__ == "__main__":
    plt.show()
