# Changes to the ColorBrewer colour schemes (Apache-2.0: changed files say so)

data/color_sequences_colorbrewer.json holds ColorBrewer's qualitative schemes Accent, Dark2, Paired, Pastel1,
Pastel2, Set1, Set2 and Set3 as Matplotlib 3.11.2 holds them (lib/matplotlib/_cm.py, the colour sequences of
lib/matplotlib/colors.py):
- each colour is the RGB triple of Brewer's spreadsheet divided by 255, as Matplotlib gives it, written as JSON
  numbers by tools/oracle/make_color_sequences.py;
- the schemes are kept at their largest number of classes only; the colours themselves are unchanged.
