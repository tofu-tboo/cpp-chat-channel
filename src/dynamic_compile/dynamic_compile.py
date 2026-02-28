import os
import re
import sys

# ANSI Color Codes
C_MAGENTA = '\033[95m'
C_YELLOW = '\033[93m'
C_CYAN = '\033[96m'
C_RESET = '\033[0m'


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
        print(f"{C_YELLOW}[WARNING]{C_RESET} Options directory not found: {OPTIONS_DIR}")
        return options_map

    for filename in os.listdir(OPTIONS_DIR):
        if not filename.endswith(".h"):
            continue
        
        filepath = os.path.join(OPTIONS_DIR, filename)
        try:
            with open(filepath, 'r', encoding='utf-8') as f:
                content = f.read()
                # Regex to find #define KEYWORD ; or #define KEYWORD(...) ;
                matches = re.findall(r'#define\s+(\w+)(?:\([^)]*\))?\s*;?', content)
                
                # Determine logic type from filename (e.g., virtual_parent.h -> virtual_parent)
                logic_type = os.path.splitext(filename)[0]
                
                for keyword in matches:
                    options_map[keyword] = logic_type
        except Exception as e:
            print(f"{C_YELLOW}[ERROR]{C_RESET} reading {filepath}: {e}")
            sys.exit(1)
            
    return options_map

def apply_virtual_parent(keyword, project_files, color):
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
            print(f"{C_YELLOW}[ERROR]{C_RESET} scanning {filepath}: {e}")
            sys.exit(1)

    if not target_classes:
        return

    print(f"{color}[{keyword}]{C_RESET} Target classes for virtual inheritance: {target_classes}")

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
                print(f"  -> Applying virtual inheritance in {filepath}")
                with open(filepath, 'w', encoding='utf-8') as f:
                    f.write(content)

        except Exception as e:
            print(f"{C_YELLOW}[ERROR]{C_RESET} processing {filepath}: {e}")
            sys.exit(1)

def apply_define_seq_linter(keyword, project_files, color):
    """
    1. Find files marked with the keyword.
    2. Extract all #define macros from those files in order.
    3. Scan all project files to check if the macros are used in the correct sequence.
    """
    lint_macros = []
    definition_files = set()
    
    # Step 1 & 2: Find linting files and extract macros
    for filepath in project_files:
        try:
            with open(filepath, 'r', encoding='utf-8') as f:
                content = f.read()
            
            if keyword in content:
                print(f"{color}[{keyword}]{C_RESET} Found in: {filepath}. Extracting macros.")
                definition_files.add(filepath)
                # Simple regex to find all #define identifiers
                found_macros = re.findall(r'#define\s+(\w+)', content)
                if found_macros:
                    lint_macros.extend(found_macros)
        except Exception as e:
            print(f"{C_YELLOW}[ERROR]{C_RESET} scanning {filepath} for linting macros: {e}")
            sys.exit(1)

    if not lint_macros:
        return
    
    # Remove the keyword itself from the list if it was captured
    if keyword in lint_macros:
        lint_macros.remove(keyword)

    print(f"{color}[{keyword}]{C_RESET} Macros to lint for sequence: {lint_macros}")
    macro_order = {macro: i for i, macro in enumerate(lint_macros)}

    # Step 3: Scan all project files and check the order
    for filepath in project_files:
        if filepath in definition_files:
            continue

        try:
            with open(filepath, 'r', encoding='utf-8') as f:
                content = f.read()
            
            # Find all occurrences of the linted macros in the current file
            # We use word boundaries (\b) to avoid matching parts of other words
            found_in_file = re.findall(r'\b(' + '|'.join(re.escape(m) for m in lint_macros) + r')\b', content)
            
            if not found_in_file:
                continue

            # Check if the sequence is correct
            last_idx = -1
            for macro in found_in_file:
                current_idx = macro_order.get(macro, -1)
                if current_idx < last_idx:
                    print(f"  {C_YELLOW}[LINT ERROR]{C_RESET} in {filepath}: Macro '{macro}' appears out of order.")
                    # You could make this an error that stops the build if needed
                    sys.exit(1)
                last_idx = current_idx
        except Exception as e:
            print(f"{C_YELLOW}[ERROR]{C_RESET} linting {filepath}: {e}")
            sys.exit(1)

def main():
    print(f"{C_CYAN}--- Dynamic Compile Start ---{C_RESET}")
    options = parse_options()
    if not options:
        print(f"{C_YELLOW}[INFO]{C_RESET} No options found.")
        return

    all_code_files = get_project_files(ALL_CODE_EXTS)
    header_files = get_project_files(HEADER_EXTS)
    
    logic_colors = {}
    color_palette = [C_CYAN, C_MAGENTA]
    color_idx = 0

    for keyword, logic_type in options.items():
        if logic_type not in logic_colors:
            logic_colors[logic_type] = color_palette[color_idx % len(color_palette)]
            color_idx += 1
        color = logic_colors[logic_type]

        if logic_type == 'virtual_parent':
            apply_virtual_parent(keyword, header_files, color)
        elif logic_type == 'def_seq_linter':
            apply_define_seq_linter(keyword, all_code_files, color)

    print(f"{C_CYAN}--- Dynamic Compile End ---{C_RESET}")

if __name__ == "__main__":
    main()