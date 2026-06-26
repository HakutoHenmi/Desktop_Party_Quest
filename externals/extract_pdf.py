import sys
import subprocess

try:
    import pypdf
except ImportError:
    subprocess.check_call([sys.executable, '-m', 'pip', 'install', 'pypdf'])
    import pypdf

reader = pypdf.PdfReader('c:/Users/k024g/source/repos/Desktop_Party_Quest/externals/Desktop_Party_Quest_Detailed_Specification.pdf')
with open('c:/Users/k024g/source/repos/Desktop_Party_Quest/externals/Desktop_Party_Quest_Detailed_Specification.txt', 'w', encoding='utf-8') as f:
    for page in reader.pages:
        text = page.extract_text()
        if text:
            f.write(text + '\n')
print("Extracted PDF to txt successfully")
