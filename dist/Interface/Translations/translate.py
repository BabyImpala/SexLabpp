import shutil
from pathlib import Path
import sys

languages = [
  "CHINESE",
  "CZECH",
  "DANISH",
  # "ENGLISH",
  "FINNISH",
  "FRENCH",
  "GERMAN",
  "GREEK",
  "ITALIAN",
  "JAPANESE",
  "NORWEGIAN",
  "POLISH",
  "RUSSIAN",
  "SPANISH",
  "SWEDISH",
  "TURKISH"
]
translated_languages = [
  "RUSSIAN",
]

script_dir = Path(__file__).parent.resolve()
if script_dir.name == "Translations":
  translations_dir = script_dir
else:
  translations_dir = script_dir / "Interface" / "Translations"

if not translations_dir.exists():
  print(f"Error: Could not find Translations directory at {translations_dir}")
  sys.exit(1)

f_english = list(translations_dir.glob("*ENGLISH.txt"))
if not f_english:
  print(f"Missing ENGLISH.txt in {translations_dir}")
  sys.exit(1)

en_path = f_english[0]
f_raw = en_path.name.replace("ENGLISH.txt", "")

def parse_file(file_path):
  with open(file_path, 'r', encoding='utf-16le') as file:
    lines = file.readlines()
  keys = {line.split(maxsplit=1)[0]: line for line in lines if line.startswith('$')}
  return keys, lines

if len(translated_languages) > 0:
  en_keys, en_lines = parse_file(en_path)

def copy_new_keys(file_path):
  l_keys, _ = parse_file(file_path)
  with open(file_path, 'w', encoding='utf-16le') as l_file:
    for line in en_lines:
      if line.startswith('$'):
        key = line.split(maxsplit=1)[0]
        if key in l_keys:
          l_file.write(l_keys[key])
        else:
          l_file.write("# TODO: " + key + "\n")
          l_file.write(en_keys[key])
      else:
        l_file.write(line)

for l in languages:
  new_path = translations_dir / (f_raw + l + ".txt")
  print(f"Processing {new_path}")
  if l in translated_languages:
    print(f"Copying new keys to {l}")
    copy_new_keys(new_path)
    continue
  shutil.copyfile(en_path, new_path)

print("Done")
