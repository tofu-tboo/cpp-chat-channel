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
    
    cpp_keywords = {
        'alignas', 'alignof', 'and', 'and_eq', 'asm', 'atomic_cancel', 'atomic_commit', 'atomic_noexcept', 
        'auto', 'bitand', 'bitor', 'bool', 'break', 'case', 'catch', 'char', 'char8_t', 'char16_t', 'char32_t', 
        'class', 'compl', 'concept', 'const', 'consteval', 'constexpr', 'constinit', 'const_cast', 'continue', 
        'co_await', 'co_return', 'co_yield', 'decltype', 'default', 'delete', 'do', 'double', 'dynamic_cast', 
        'else', 'enum', 'explicit', 'export', 'extern', 'false', 'float', 'for', 'friend', 'goto', 'if', 'inline', 
        'int', 'long', 'mutable', 'namespace', 'new', 'noexcept', 'not', 'not_eq', 'nullptr', 'operator', 'or', 
        'or_eq', 'private', 'protected', 'public', 'reflexpr', 'register', 'reinterpret_cast', 'requires', 
        'return', 'short', 'signed', 'sizeof', 'static', 'static_assert', 'static_cast', 'struct', 'switch', 
        'synchronized', 'template', 'this', 'thread_local', 'throw', 'true', 'try', 'typedef', 'typeid', 
        'typename', 'union', 'unsigned', 'using', 'virtual', 'void', 'volatile', 'wchar_t', 'while', 'xor', 
        'xor_eq', 'override', 'final'
    }
    
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
            local_brace_depth = 0

            while i < len(lines):
                line = lines[i].strip()
                i += 1
                if not line: continue

                open_braces = line.count('{')
                close_braces = line.count('}')

                if local_brace_depth > 0:
                    local_brace_depth += open_braces - close_braces
                    continue

                if any(line == spec for spec in private_specifiers):
                    current_access = 'private'
                    local_brace_depth += open_braces - close_braces
                    continue
                if any(line == spec for spec in public_protected_specifiers):
                    current_access = 'public'
                    local_brace_depth += open_braces - close_braces
                    continue

                if current_access == 'private': 
                    local_brace_depth += open_braces - close_braces
                    continue

                if line.startswith(('using ', 'typedef ', 'enum ', 'friend ', 'SET_SUPER', '__USING_SUPER_MEM__', '#')): 
                    local_brace_depth += open_braces - close_braces
                    continue

                terminator_pos = len(line)
                if '(' in line: terminator_pos = min(terminator_pos, line.find('('))
                if ';' in line: terminator_pos = min(terminator_pos, line.find(';'))
                if '=' in line and '<' not in line[:line.find('=')]:
                    terminator_pos = min(terminator_pos, line.find('='))
                if '{' in line: terminator_pos = min(terminator_pos, line.find('{'))

                declaration_part = line[:terminator_pos].strip()
                
                local_brace_depth += open_braces - close_braces

                if not declaration_part: continue

                # Skip nested class/struct definitions
                if re.match(r'^\s*(template\s*<[^>]*>\s*)?(class|struct)\s+\w+(\s*:\s*.*)?$', declaration_part):
                    continue

                # Extract member name, which is usually the last word before '(', ';', or '='
                # This is a simplified regex and might not cover all edge cases.
                match_name = re.search(r'(\w+)\s*$', declaration_part)
                if not match_name: continue
                
                member_name = match_name.group(1)

                if (member_name and 
                    member_name != class_name and 
                    not member_name.startswith('~') and
                    member_name not in cpp_keywords):
                    members[member_name] = members.get(member_name, 0) + 1

            return members

        except Exception:
            continue
    return members

def apply_expose_template_mem(keyword, project_files, color, all_code_files):
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

            original_content = content
            
            # Find all top-level class/struct definitions
            class_regions = []
            class_start_regex = re.compile(r'(template\s*<[^>]*>\s*)?(class|struct)\s+([^:{]+)\s*(:[^\{]*)?\{')
            
            for match in class_start_regex.finditer(content):
                # Heuristic to skip nested classes: check if we are already inside a { } block
                pre_match_content = content[:match.start()]
                if pre_match_content.count('{') > pre_match_content.count('}'):
                    continue

                class_content_start_pos = match.end()
                brace_count = 1
                end_pos = class_content_start_pos
                
                while brace_count > 0 and end_pos < len(content):
                    if content[end_pos] == '{': brace_count += 1
                    elif content[end_pos] == '}': brace_count -= 1
                    end_pos += 1
                
                class_regions.append({'match': match, 'end': end_pos})

            if not class_regions:
                continue

            # Cache implementation file contents for this header
            impl_files_content = {}
            file_dir = os.path.dirname(filepath)
            file_basename = os.path.splitext(os.path.basename(filepath))[0]
            for f_path in all_code_files:
                if os.path.dirname(f_path) == file_dir and os.path.splitext(os.path.basename(f_path))[0] == file_basename:
                    try:
                        with open(f_path, 'r', encoding='utf-8') as f:
                            impl_files_content[f_path] = f.read()
                    except Exception:
                        pass

            # Process regions in reverse to not mess up indices
            for region in reversed(class_regions):
                match = region['match']
                class_start_pos = match.start()
                class_end_pos = region['end']
                
                class_full_text = content[class_start_pos:class_end_pos]
                
                if keyword not in class_full_text:
                    continue

                child_class_name_full = match.group(3).strip()
                child_class_name_base_match = re.match(r'([\w:]+)', child_class_name_full)
                if not child_class_name_base_match:
                    continue
                child_class_name = child_class_name_base_match.group(1)

                inheritance_str_with_colon = match.group(4)
                
                if not inheritance_str_with_colon: continue
                
                print(f"{color}[{keyword}]{C_RESET} Processing {child_class_name} in {filepath}")

                inheritance_str = inheritance_str_with_colon[1:]
                
                # Clean comments from inheritance string
                inheritance_str = re.sub(r'//.*', '', inheritance_str)
                inheritance_str = re.sub(r'/\*.*?\*/', '', inheritance_str, flags=re.DOTALL)
                inheritance_str = inheritance_str.strip()

                # Parse inheritance string to handle templates with commas correctly
                parents = []
                current_parent = []
                angle_level = 0
                for char in inheritance_str:
                    if char == '<': angle_level += 1
                    elif char == '>': angle_level -= 1
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
                    if prev == full_parent_name: break
                
                full_parent_name = full_parent_name.strip()
                
                base_name_match = re.match(r'([\w:]+)', full_parent_name)
                if not base_name_match: continue
                parent_base_name = base_name_match.group(1)
                if '::' in parent_base_name:
                    parent_base_name = parent_base_name.split('::')[-1]

                # Build implementation content for this specific class check
                impl_content = ""
                for f_path, f_content in impl_files_content.items():
                    cnt = f_content
                    if f_path == filepath:
                        # Use class_full_text for header to restrict scope
                        cnt = class_full_text
                        
                        block_regex_strip = re.compile(
                            re.escape(keyword) + r'((?:\s*using\s+' + re.escape(full_parent_name) + r'::\w+;)*)'
                        )
                        cnt = block_regex_strip.sub('', cnt)
                    
                    cnt = re.sub(r'//.*', '', cnt)
                    cnt = re.sub(r'/\*.*?\*/', '', cnt, flags=re.DOTALL)
                    impl_content += cnt + "\n"

                all_headers = get_project_files(HEADER_EXTS)
                parent_members = find_class_members(parent_base_name, all_headers, all_class_dot_h_macros)
                child_members = find_class_members(child_class_name, [filepath], all_class_dot_h_macros)

                members_to_use = set()
                for name, p_count in parent_members.items():
                    c_count = child_members.get(name, 0)
                    # Strict check: ensure name is not preceded by ., ->, or ::
                    if (c_count == 0 or p_count > c_count) and re.search(r'(?<!\.)(?<!->)(?<!::)\b' + re.escape(name) + r'\b', impl_content):
                        members_to_use.add(name)

                # Find the indentation of the line where the keyword is located, for consistent formatting.
                keyword_pos_in_class = class_full_text.find(keyword)
                indent = '\t' # Fallback indent
                if keyword_pos_in_class != -1:
                    line_start = class_full_text.rfind('\n', 0, keyword_pos_in_class) + 1
                    line_content = class_full_text[line_start:]
                    indent_match = re.match(r'(\s*)', line_content)
                    if indent_match:
                        indent = indent_match.group(1)

                # Generate a clean, multi-line block of 'using' statements.
                new_block = keyword
                if members_to_use:
                    using_lines = [f"{indent}using {full_parent_name}::{member};" for member in sorted(list(members_to_use))]
                    new_block += "\n" + "\n".join(using_lines)

                # Use a flexible regex that can match both single-line and multi-line existing blocks.
                block_regex = re.compile(
                    re.escape(keyword) + r'((?:\s*using\s+' + re.escape(full_parent_name) + r'::\w+;)*)'
                )
                
                block_match_in_class = block_regex.search(class_full_text)

                if not block_match_in_class:
                    print(f"  {C_YELLOW}[WARNING]{C_RESET} Could not find a replaceable block for {keyword} in {child_class_name}. Inserting fresh.")
                    new_class_text = class_full_text.replace(keyword, new_block, 1)
                else:
                    existing_block = block_match_in_class.group(0)
                    if existing_block.strip() == new_block.strip():
                        print(f"  -> Declarations for {child_class_name} are already up-to-date.")
                        continue
                    new_class_text = class_full_text.replace(existing_block, new_block, 1)
                
                content = content[:class_start_pos] + new_class_text + content[class_end_pos:]
                print(f"  -> Updated using declarations for {len(members_to_use)} members in {child_class_name}.")

            if content != original_content:
                with open(filepath, 'w', encoding='utf-8') as f:
                    f.write(content)
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
            apply_expose_template_mem(keyword, header_files, color, all_code_files)

    print(f"{C_CYAN}--- Dynamic Compile End ---{C_RESET}")

if __name__ == "__main__":
    main()