import os
import re

# Paths
CURRENT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.abspath(os.path.join(CURRENT_DIR, "../.."))
SRC_DIR = os.path.join(PROJECT_ROOT, "src")
OPTIONS_DIR = os.path.join(CURRENT_DIR, "options")

# Extensions to scan
ALL_CODE_EXTS = {'.c', '.cpp', '.h', '.hpp', '.tpp'}
HEADER_EXTS = {'.h', '.hpp'}

def get_project_files(target_exts):
    """
    Scans the project source directory and returns a list of files
    with the specified extensions.
    """
    files_list = []
    for root, _, files in os.walk(SRC_DIR):
        for file in files:
            if os.path.splitext(file)[1] in target_exts:
                files_list.append(os.path.join(root, file))
    return files_list


def parse_options():
    """
    Scans options folder for header files.
    Finds #define KEYWORD ; patterns.
    Returns dict: { keyword: logic_type }
    """
    options_map = {}
    if not os.path.exists(OPTIONS_DIR):
        print(f"Options directory not found: {OPTIONS_DIR}")
        return options_map

    for filename in os.listdir(OPTIONS_DIR):
        if not filename.endswith(".h"):
            continue
        
        filepath = os.path.join(OPTIONS_DIR, filename)
        try:
            with open(filepath, 'r', encoding='utf-8') as f:
                content = f.read()
                # Regex to find #define KEYWORD ; or #define KEYWORD(...) ;
                matches = re.findall(r'#define\s+(\w+)(?:\([^)]*\))?\s+;', content)
                
                # Determine logic type from filename (e.g., virtual_parent.h -> virtual_parent)
                logic_type = os.path.splitext(filename)[0]
                
                for keyword in matches:
                    options_map[keyword] = logic_type
        except Exception as e:
            print(f"Error reading {filepath}: {e}")
            
    return options_map

def apply_virtual_parent(keyword, project_files):
    """
    1. Find classes marked with keyword.
    2. Find derived classes.
    3. Add 'virtual' to inheritance.
    """
    target_classes = set()
    
    # Step 1: Find target classes (e.g., __virtual_parent__ class Loggable)
    for filepath in project_files: # Already filtered for headers
        try:
            with open(filepath, 'r', encoding='utf-8') as f:
                content = f.read()
            
            # Pattern: keyword followed by class/struct and name
            pattern = re.compile(re.escape(keyword) + r'\s+(?:class|struct)\s+(\w+)')
            found = pattern.findall(content)
            for cls in found:
                target_classes.add(cls)
        except Exception as e:
            print(f"Error scanning {filepath}: {e}")

    if not target_classes:
        return

    print(f"[{keyword}] Target classes for virtual inheritance: {target_classes}")

    # Step 2 & 3: Modify inheritance in derived classes
    for filepath in project_files:
        try:
            with open(filepath, 'r', encoding='utf-8') as f:
                content = f.read()
            
            original_content = content
            modified = False

            for target in target_classes:
                # Regex to find inheritance declaration:
                # Matches ':' or ',' followed by optional access specifier, then target class name.
                # Negative lookahead (?!\s*\() ensures we don't match constructor initializer lists.
                
                escaped_target = re.escape(target)
                regex = re.compile(r'([:|,])(\s*(?:public\s+|protected\s+|private\s+)?\s*)(\b' + escaped_target + r'\b)(?!\s*\()')
                
                def replacement(match):
                    delimiter = match.group(1)
                    prefix = match.group(2)
                    cls_name = match.group(3)
                    
                    # If prefix contains 'virtual', do nothing
                    if 'virtual' in prefix:
                        return match.group(0)
                    
                    # Construct new prefix with virtual
                    access_match = re.search(r'(public|protected|private)', prefix)
                    if access_match:
                        # e.g. " public " -> " virtual public "
                        new_prefix = prefix.replace(access_match.group(1), 'virtual ' + access_match.group(1))
                    else:
                        # No access specifier. e.g. " " -> " virtual "
                        if not prefix.strip():
                            new_prefix = prefix + 'virtual '
                        else:
                            new_prefix = ' virtual ' + prefix.strip() + ' '
                            
                    return f"{delimiter}{new_prefix}{cls_name}"

                new_content = regex.sub(replacement, content)
                if new_content != content:
                    content = new_content
                    modified = True

            if modified:
                print(f"Applying virtual inheritance in {filepath}")
                with open(filepath, 'w', encoding='utf-8') as f:
                    f.write(content)

        except Exception as e:
            print(f"Error processing {filepath}: {e}")

def main():
    print("--- Dynamic Compile Start ---")
    options = parse_options()
    if not options:
        print("No options found.")
        return

    all_code_files = get_project_files(ALL_CODE_EXTS)
    header_files = get_project_files(HEADER_EXTS)
    
    for keyword, logic_type in options.items():
        if logic_type == 'virtual_parent':
            apply_virtual_parent(keyword, header_files)
        else:
            # Placeholder for other logic types (e.g., derived_friend)
            pass

    print("--- Dynamic Compile End ---")

if __name__ == "__main__":
    main()