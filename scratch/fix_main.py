import os

path = r'c:\tridjs\Tridjs-Live-Suite-main\Source\MainComponent.cpp'
# Try to read with different encodings
encodings = ['utf-8', 'utf-16', 'latin-1']
content = None
for enc in encodings:
    try:
        with open(path, 'r', encoding=enc) as f:
            content = f.read()
        print(f"Read with {enc}")
        break
    except:
        continue

if content:
    # Replace the block
    old_block = """      if (ev.type != ControllerEventType::Unknown)
          controllerRouter.pushEvent(ev);
  };"""
    
    new_block = """      if (ev.type != ControllerEventType::Unknown)
          controllerRouter.pushEvent(ev);

      if (ctrl.isScriptBinding && jsEngine)
          jsEngine->callFunction(ctrl.key, (float)val7bit, ctrl.group, ctrl.key);
  };"""
    
    # Try to find even if whitespace is different
    import re
    # Match loosely
    pattern = re.compile(r'if\s*\(ev\.type\s*!=\s*ControllerEventType::Unknown\)\s*controllerRouter\.pushEvent\(ev\);\s*};', re.MULTILINE)
    
    match = pattern.search(content)
    if match:
        print("Found match!")
        new_content = content[:match.start()] + new_block + content[match.end():]
        with open(path, 'w', encoding='utf-8') as f:
            f.write(new_content)
        print("File updated!")
    else:
        print("Pattern not found even with regex.")
        # Print a slice around where we think it is
        idx = content.find("controllerRouter")
        if idx != -1:
            print(f"Context: {content[idx-50:idx+50]}")
