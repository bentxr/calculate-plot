# Written by calculate-plot 0.1.0: this script draws the same figure with Matplotlib 3.11.2.
import locale

import matplotlib.pyplot as plt

# Text is shaped for the language of the locale: this is the one the figure was made with.
try:
    locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
except locale.Error:
    locale.setlocale(locale.LC_CTYPE, "C")

plt.rcParams.update({
    'text.language': 'es',
})
fig, axs = plt.subplots(1, 2, figsize=(9.0, 5.0))
axs[0].plot([0, 10], [0, 10], label='a fine fit: efficient')
axs[0].plot([0, 10], [10, 0], label='office\nfifty-five')
axs[0].set_title('Default figure: efficient office')
axs[0].set_xlabel('first\nfine line')
axs[0].set_ylabel('affine coefficient', language='en')
axs[0].text(5, 9, "affine fit: the rcParams' language", ha='center')
axs[0].text(5, 8, 'affine fit: language None', ha='center', language=None)
axs[0].text(5, 7, 'affine fit: language en', ha='center', language='en')
axs[0].text(5, 6, 'affine fit: an empty language', ha='center', language='')
axs[0].text(5, 4, 'affine fit: es-ES', ha='right', language='es-ES')
axs[0].text(5, 3, 'affine fit: ES', ha='right', language='ES')
axs[0].text(5, 2, 'affine fit: C.UTF-8', ha='right', language='C.UTF-8')
axs[0].text(5, 1, 'affine fit: no such language', ha='right', language='no such language')
axs[0].legend(loc='lower right', title='fitted figures')
axs[1].set_title('Default figure: efficient office', language='en', loc='left')
axs[1].set_xlabel('efficient office', language='en')
axs[1].set_xlabel('efficient office', language=None)
axs[1].set_ylabel('efficient office', language=None)
axs[1].set_ylabel('efficient office', language='en')
axs[1].text(0.5, 0.9, 'бгдпт fi: sr', ha='center', language='sr', fontsize=16)
axs[1].text(0.5, 0.8, 'бгдпт fi: ru', ha='center', language='ru', fontsize=16)
axs[1].text(0.5, 0.7, 'бгдпт fi: sr, italic', ha='center', language='sr', fontsize=16, style='italic')
axs[1].text(0.5, 0.6, 'бгдпт fi: italic', ha='center', fontsize=16, style='italic')
axs[1].text(0.5, 0.5, 'ŞşŢţ fi: ro', ha='center', language='ro', fontsize=16)
axs[1].text(0.5, 0.4, 'ŞşŢţ fi: tr, serif', ha='center', language='tr', fontsize=16, family='serif')
axs[1].text(0.5, 0.3, 'fi fl ffi: nl, monospace', ha='center', language='nl', family='monospace')
axs[1].text(0.5, 0.2, 'two lines: fi\nthe language is not used: affine', ha='center', language='en')
axs[1].text(0.5, 0.05, '$fi$ math fi: en', ha='center', language='en')
fig.suptitle('A figure of fine fits', language='tr')
fig.text(0.02, 0.02, 'fig.text: office', language='en')
fig.text(0.98, 0.02, 'fig.text: office', ha='right')

if __name__ == "__main__":
    plt.show()
