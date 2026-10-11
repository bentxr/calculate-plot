# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import matplotlib.pyplot as plt

plt.rcParams.update({
    'mathtext.fontset': 'stix',
})
fig, ax = plt.subplots(figsize=(6.0, 4.5))
ax.set_xlim(0, 10)
ax.set_ylim(0, 10)
ax.text(0.5, 9, "the rcParams' fontset: $\\sum_{i}\\frac{x_i^2}{\\sqrt{\\alpha}}$")
ax.text(0.5, 7.8, 'dejavusans: $\\sum_{i}\\frac{x_i^2}{\\sqrt{\\alpha}}$', math_fontfamily='dejavusans')
ax.text(0.5, 6.6, 'dejavuserif: $\\sum_{i}\\frac{x_i^2}{\\sqrt{\\alpha}}$', math_fontfamily='dejavuserif')
ax.text(0.5, 5.4, 'cm: $\\sum_{i}\\frac{x_i^2}{\\sqrt{\\alpha}}$', math_fontfamily='cm')
ax.text(0.5, 4.2, 'stix: $\\sum_{i}\\frac{x_i^2}{\\sqrt{\\alpha}}$', math_fontfamily='stix')
ax.text(0.5, 3.0, 'stixsans: $\\sum_{i}\\frac{x_i^2}{\\sqrt{\\alpha}}$', math_fontfamily='stixsans')
ax.text(0.5, 1.8, 'custom: $\\sum_{i}\\frac{x_i^2}{\\sqrt{\\alpha}}$', math_fontfamily='custom')
ax.text(0.5, 0.6, 'None: $\\sum_{i}\\frac{x_i^2}{\\sqrt{\\alpha}}$', math_fontfamily=None, family='serif')
ax.set_title("a title in the rcParams' fontset: $\\mathcal{L}=\\int\\hbar$")

if __name__ == "__main__":
    plt.show()
