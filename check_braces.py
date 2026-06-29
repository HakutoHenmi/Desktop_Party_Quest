import sys
with open(r'c:\Users\k024g\source\repos\Desktop_Party_Quest\Game\Scenes\MainScene.cpp', encoding='utf-8') as f:
    text = f.read()

depth = 0
for i, c in enumerate(text):
    if c == '{': depth += 1
    if c == '}': depth -= 1
    if depth == 0 and i > 5000:
        lines = text[:i+1].count('\n') + 1
        print('Zero depth at line', lines)
        sys.exit(0)
