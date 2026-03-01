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
    3. Scan all project files to check if the macros are used in the correct sequence within each top-level class.
    """
    if keyword.endswith("_END__"):
        return

    end_keyword = keyword[:-2] + "_END__"
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
                
                pattern = re.compile(re.escape(keyword) + r'(.*?)' + re.escape(end_keyword), re.DOTALL)
                match = pattern.search(content)
                if match:
                    found_macros = re.findall(r'#define\s+(\w+)', match.group(1))
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
    lint_macros_re = re.compile(r'\b(' + '|'.join(re.escape(m) for m in lint_macros) + r')\b')
    class_start_re = re.compile(r'^\s*(template\s*<[^>]*>\s*)?(class|struct)\s+')

    # Step 3: Scan all project files and check the order
    for filepath in project_files:
        if filepath in definition_files:
            continue

        try:
            with open(filepath, 'r', encoding='utf-8') as f:
                content = f.read()
            
            # Pre-process content to remove comments and simplify parsing
            clean_content = re.sub(r'//.*', '', content)
            clean_content = re.sub(r'/\*.*?\*/', '', clean_content, flags=re.DOTALL)

            nesting_level = 0
            last_macro_idx = -1
            
            lines = clean_content.splitlines()
            for i, line in enumerate(lines):
                # Check for class/struct start at top level *before* processing the line
                if nesting_level == 0 and class_start_re.match(line):
                    # New top-level class/struct found, reset the linter state
                    last_macro_idx = -1
                
                # Find macros on the current line
                found_macros = lint_macros_re.findall(line)
                for macro in found_macros:
                    current_idx = macro_order.get(macro, -1)
                    if current_idx < last_macro_idx:
                        print(f"  {C_YELLOW}[LINT ERROR]{C_RESET} in {filepath} on line {i+1}: Macro '{macro}' appears out of order.")
                        sys.exit(1)
                    last_macro_idx = current_idx
                
                # Update nesting level based on braces *after* processing the line
                nesting_level += line.count('{')
                nesting_level -= line.count('}')
                if nesting_level < 0:
                    nesting_level = 0
        except Exception as e:
            print(f"{C_YELLOW}[ERROR]{C_RESET} linting {filepath}: {e}")
            sys.exit(1)

def find_class_members(class_name, files_to_search, all_class_dot_h_macros):
    """
    Finds public and protected members of a given class.
    Excludes constructors, destructors, and private members.
    Returns a dict {member_name: count} to handle overloading.
    """
    members = {}
    
    private_specifiers = {'private:'}
    public_protected_specifiers = {'public:', 'protected:'}
    for macro in all_class_dot_h_macros:
        if 'private' in macro: private_specifiers.add(macro + ':')
        elif 'public' in macro or 'protected' in macro: public_protected_specifiers.add(macro + ':')

    for filepath in files_to_search:
        try:
            with open(filepath, 'r', encoding='utf-8') as f:
                content = f.read()
            
            class_regex = re.compile(r'(class|struct)\s+' + re.escape(class_name) + r'\s*(?::[^\{]*)?\{')
            match = class_regex.search(content)
            if not match: continue

            # Found the class, now parse it
            start_pos = match.end()
            brace_count = 1
            end_pos = start_pos
            while brace_count > 0 and end_pos < len(content):
                if content[end_pos] == '{': brace_count += 1
                elif content[end_pos] == '}': brace_count -= 1
                end_pos += 1
            class_body = content[start_pos:end_pos-1]

            class_body = re.sub(r'//.*', '', class_body)
            class_body = re.sub(r'/\*.*?\*/', '', class_body, flags=re.DOTALL)

            current_access = 'private' if match.group(1) == 'class' else 'public'

            lines = class_body.splitlines()
            i = 0
            while i < len(lines):
                line = lines[i].strip()
                i += 1
                if not line: continue

                if any(line == spec for spec in private_specifiers):
                    current_access = 'private'
                    continue
                if any(line == spec for spec in public_protected_specifiers):
                    current_access = 'public'
                    continue

                if current_access == 'private': continue

                # Skip nested struct/class definitions to avoid parsing their members
                if (line.startswith('struct ') or line.startswith('class ')) and '{' in line:
                    brace_count = line.count('{') - line.count('}')
                    while i < len(lines) and brace_count > 0:
                        brace_count += lines[i].count('{') - lines[i].count('}')
                        i += 1
                    continue

                if line.startswith(('using ', 'typedef ', 'enum ', 'friend ', 'SET_SUPER', '__USING_SUPER_MEM__')): continue

                terminator_pos = len(line)
                if '(' in line: terminator_pos = min(terminator_pos, line.find('('))
                if ';' in line: terminator_pos = min(terminator_pos, line.find(';'))
                if '=' in line and '<' not in line[:line.find('=')]:
                    terminator_pos = min(terminator_pos, line.find('='))

                declaration_part = line[:terminator_pos].strip()
                if not declaration_part: continue

                # Extract member name, which is usually the last word before '(', ';', or '='
                # This is a simplified regex and might not cover all edge cases.
                match_name = re.search(r'(\w+)\s*$', declaration_part)
                if not match_name: continue
                
                member_name = match_name.group(1)

                if (member_name and 
                    member_name != class_name and 
                    not member_name.startswith('~') and
                    not member_name.startswith('operator')):
                    members[member_name] = members.get(member_name, 0) + 1

            return members

        except Exception:
            continue
    return members

def apply_expose_template_mem(keyword, project_files, color):
    class_h_path = os.path.join(SRC_DIR, "libs", "class.h")
    all_class_dot_h_macros = []
    try:
        with open(class_h_path, 'r', encoding='utf-8') as f:
            all_class_dot_h_macros = re.findall(r'#define\s+(\w+)', f.read())
    except Exception as e:
        print(f"{C_YELLOW}[ERROR]{C_RESET} Could not read {class_h_path}: {e}")
        sys.exit(1)

    for filepath in project_files:
        try:
            with open(filepath, 'r', encoding='utf-8') as f:
                content = f.read()
            
            if keyword not in content: continue

            child_class_match = re.search(r'class\s+(\w+)\s*:', content)
            if not child_class_match: continue
            child_class_name = child_class_match.group(1)

            # Find parent class from inheritance declaration
            # class Child : public Parent<T>, ... {
            inheritance_match = re.search(r'class\s+' + re.escape(child_class_name) + r'\s*:\s*([^{]+)\{', content)
            if not inheritance_match: continue
            
            inheritance_str = inheritance_match.group(1)
            
            # Clean comments
            inheritance_str = re.sub(r'//.*', '', inheritance_str)
            inheritance_str = re.sub(r'/\*.*?\*/', '', inheritance_str, flags=re.DOTALL)

            # Parse inheritance string to handle templates with commas correctly
            inheritance_str = inheritance_str.strip()
            parents = []
            current_parent = []
            angle_level = 0
            for char in inheritance_str:
                if char == '<':
                    angle_level += 1
                elif char == '>':
                    angle_level -= 1
                
                if char == ',' and angle_level == 0:
                    parents.append("".join(current_parent).strip())
                    current_parent = []
                else:
                    current_parent.append(char)
            if current_parent:
                parents.append("".join(current_parent).strip())
            
            if not parents: continue

            full_parent_name = parents[0]
            
            # Remove access specifiers and 'virtual'
            while True:
                prev = full_parent_name
                full_parent_name = re.sub(r'^\s*(public|protected|private|virtual)\s+', '', full_parent_name)
                if prev == full_parent_name:
                    break
            
            full_parent_name = full_parent_name.strip()
            
            base_name_match = re.match(r'([\w:]+)', full_parent_name)
            if not base_name_match: continue
            parent_base_name = base_name_match.group(1)
            if '::' in parent_base_name:
                parent_base_name = parent_base_name.split('::')[-1]
            
            print(f"{color}[{keyword}]{C_RESET} Processing {child_class_name} in {filepath}")

            all_headers = get_project_files(HEADER_EXTS)
            parent_members = find_class_members(parent_base_name, all_headers, all_class_dot_h_macros)
            child_members = find_class_members(child_class_name, [filepath], all_class_dot_h_macros)

            members_to_use = set()
            for name, p_count in parent_members.items():
                c_count = child_members.get(name, 0)
                # 1. If child doesn't have it at all -> use it.
                # 2. If child has it, but parent has more overloads -> use it to unhide parent's overloads.
                if c_count == 0 or p_count > c_count:
                    members_to_use.add(name)
            
            if not members_to_use: continue

            keyword_line_match = re.search(r'^(\s*)' + re.escape(keyword), content, re.MULTILINE)
            indent = keyword_line_match.group(1) if keyword_line_match else "\t"

            # --- New "Block Replacement" Logic ---

            # 1. Define the regex for the block to be replaced.
            # This matches the keyword, followed by any number of 'using Parent::member;' lines.
            block_regex = re.compile(
                re.escape(keyword) + r'((?:\n\s*using\s+' + re.escape(full_parent_name) + r'::\w+;)*)'
            )
            
            # 2. Generate the new, correct block of code.
            new_block = keyword
            if members_to_use:
                using_lines = [f"{indent}using {full_parent_name}::{member};" for member in sorted(list(members_to_use))]
                new_block += "".join(using_lines) # **절대 이 부분의 포맷을 고치지 말 것**

            # 3. Find the existing block in the content.
            match = block_regex.search(content)
            if not match:
                print(f"  {C_YELLOW}[WARNING]{C_RESET} Could not find a replaceable block for {keyword}. Inserting fresh.")
                new_content = content.replace(keyword, new_block, 1)
            else:
                existing_block = match.group(0)
                # 4. Compare and replace if necessary.
                if existing_block.strip() == new_block.strip():
                    print(f"  -> Declarations are already up-to-date.")
                    continue
                new_content = content.replace(existing_block, new_block, 1)
            
            with open(filepath, 'w', encoding='utf-8') as f:
                f.write(new_content)
            
            print(f"  -> Updated using declarations for {len(members_to_use)} members.")

        except Exception as e:
            print(f"{C_YELLOW}[ERROR]{C_RESET} processing {filepath}: {e}")
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
        elif logic_type == 'expose_template_mem':
            apply_expose_template_mem(keyword, header_files, color)

    print(f"{C_CYAN}--- Dynamic Compile End ---{C_RESET}")

if __name__ == "__main__":
    main()