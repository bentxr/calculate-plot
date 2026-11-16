# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

fig, axs = plt.subplots(1, 2, figsize=(9.0, 5.0))
axs[0].set_title('office fly: -liga', fontfeatures=['-liga'])
axs[0].set_xlabel('affine fit: -liga, -kern', fontfeatures=('-liga', '-kern'))
axs[0].set_ylabel('office fly: none', fontfeatures=None)
axs[0].text(0.5, 0.9, 'office fly: default', ha='center', fontsize=14)
axs[0].text(0.5, 0.8, 'office fly: -liga', ha='center', fontsize=14, fontfeatures=['-liga'])
axs[0].text(0.5, 0.7, 'office fly: liga=0', ha='center', fontsize=14, fontfeatures=['liga=0'])
axs[0].text(0.5, 0.6, 'office fly: empty', ha='center', fontsize=14, fontfeatures=[])
axs[0].text(0.5, 0.5, 'office fly: a string', ha='center', fontsize=14, fontfeatures='liga')
axs[0].text(0.5, 0.4, 'office fly: serif', ha='center', fontsize=14, family='serif', fontfeatures=['-liga'])
axs[0].text(0.5, 0.3, 'two lines: fi\noffice fly', ha='center', fontfeatures=['-liga'])
axs[0].text(0.5, 0.15, '$fi$ math fi', ha='center', fontfeatures=['-liga'])
axs[0].annotate('fi fl: annotate', (0.2, 0.05), fontfeatures=['-liga'])
axs[0].set_xticks([0.25, 0.75], labels=['fi', 'fl'], fontfeatures=['-liga'])
axs[1].set_title('бгдпт бгдпт', language=[('sr', 0, 5), ('ru', 6, 11)])
axs[1].text(0.5, 0.9, 'бгдпт бгдпт: sr, ru', ha='center', fontsize=16, language=[('sr', 0, 5), ('ru', 6, 11)])
axs[1].text(0.5, 0.8, 'бгдпт бгдпт: ru, sr', ha='center', fontsize=16, language=(('ru', 0, 5), ('sr', 6, 11)))
axs[1].text(0.5, 0.7, 'бгдпт бгдпт: sr in part', ha='center', fontsize=16, language=[('sr', 2, 8)])
axs[1].text(0.5, 0.6, 'бгдпт бгдпт: overlaps', ha='center', fontsize=16, language=[('sr', 0, 11), ('ru', 3, 8)])
axs[1].text(0.5, 0.5, 'бгдпт бгдпт: no ranges', ha='center', fontsize=16, language=[])
axs[1].text(0.5, 0.4, 'бгдпт бгдпт: unknown, sr', ha='center', fontsize=16, language=[('no such language', 0, 5), ('sr', 6, 11)])
axs[1].text(0.5, 0.3, 'бгдпт fi: sr, -liga', ha='center', fontsize=16, language='sr', fontfeatures=['-liga'])
axs[1].text(0.5, 0.2, 'бгдпт: italic, sr, true', ha='center', fontsize=16, style='italic', language=[('sr', True, 5)])
fig.suptitle('бгдпт office: sr, -liga', language=[('sr', 0, 5)], fontfeatures=['-liga'])
fig.text(0.02, 0.02, 'fig.text: office', fontfeatures=['-liga'])

if __name__ == "__main__":
    plt.show()
