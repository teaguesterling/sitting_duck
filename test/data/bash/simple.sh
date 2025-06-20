#!/bin/bash

# Simple bash script demonstrating various language constructs

# Global variables
SCRIPT_NAME="simple.sh"
VERSION="1.0"
DEBUG=false
declare -a FILES_PROCESSED
declare -A CONFIG_MAP

# Function definitions
function show_usage() {
    cat << EOF
Usage: $0 [OPTIONS] [FILES...]
    
OPTIONS:
    -h, --help      Show this help message
    -v, --verbose   Enable verbose output
    -d, --debug     Enable debug mode
    -c, --config    Configuration file path
    
EXAMPLES:
    $0 -v file1.txt file2.txt
    $0 --debug --config /etc/myconfig
EOF
}

# Function with local variables and parameter handling
process_file() {
    local file_path="$1"
    local -r backup_dir="/tmp/backups"
    local counter=0
    
    # Parameter validation
    if [[ -z "$file_path" ]]; then
        echo "Error: No file path provided" >&2
        return 1
    fi
    
    # Check if file exists and is readable
    if [[ ! -f "$file_path" || ! -r "$file_path" ]]; then
        echo "Warning: Cannot read file $file_path" >&2
        return 2
    fi
    
    # File processing with command substitution
    local file_size=$(stat -c %s "$file_path" 2>/dev/null)
    local line_count=$(wc -l < "$file_path")
    
    # Arithmetic expansion
    counter=$((counter + 1))
    FILES_PROCESSED+=("$file_path")
    
    # String manipulation
    local filename="${file_path##*/}"
    local extension="${filename##*.}"
    local basename="${filename%.*}"
    
    # Conditional processing
    case "$extension" in
        txt|log)
            echo "Processing text file: $filename"
            ;;
        sh|bash)
            echo "Processing shell script: $filename"
            # Check for shebang
            if head -n1 "$file_path" | grep -q '^#!'; then
                echo "  Has shebang line"
            fi
            ;;
        *)
            echo "Processing generic file: $filename"
            ;;
    esac
    
    # Array operations
    CONFIG_MAP["$basename"]="$file_size"
    
    return 0
}

# Function demonstrating various test operations
run_system_checks() {
    local system_ok=true
    
    echo "Running system checks..."
    
    # File test operations
    if [[ -d "/tmp" ]]; then
        echo "✓ /tmp directory exists"
    else
        echo "✗ /tmp directory missing"
        system_ok=false
    fi
    
    # Command existence check
    if command -v git >/dev/null 2>&1; then
        echo "✓ Git is available"
        local git_version=$(git --version | cut -d' ' -f3)
        echo "  Version: $git_version"
    else
        echo "✗ Git not found"
    fi
    
    # Numeric comparisons
    local disk_usage=$(df / | awk 'NR==2 {print $5}' | sed 's/%//')
    if [[ $disk_usage -lt 90 ]]; then
        echo "✓ Disk usage OK ($disk_usage%)"
    else
        echo "⚠ High disk usage: $disk_usage%"
    fi
    
    # String comparisons with regex
    if [[ $(uname -s) =~ ^Linux$ ]]; then
        echo "✓ Running on Linux"
    elif [[ $(uname -s) == "Darwin" ]]; then
        echo "✓ Running on macOS"
    else
        echo "? Unknown operating system: $(uname -s)"
    fi
    
    $system_ok && echo "All checks passed" || echo "Some checks failed"
}

# Advanced function with subshell and pipeline
analyze_files() {
    local pattern="$1"
    shift  # Remove first argument
    local files=("$@")
    
    echo "Analyzing files matching pattern: $pattern"
    
    # Process files in subshell to preserve working directory
    (
        cd /tmp || exit 1
        
        # Find and process files with pipeline
        find . -name "$pattern" -type f 2>/dev/null | while IFS= read -r file; do
            echo "Found: $file"
            # File analysis with multiple commands
            {
                echo "  Size: $(stat -c %s "$file" 2>/dev/null || echo "unknown")"
                echo "  Modified: $(stat -c %y "$file" 2>/dev/null || echo "unknown")"
                echo "  Type: $(file -b "$file" 2>/dev/null || echo "unknown")"
            } | sed 's/^/    /'
        done
    )
    
    # Process provided files array
    for file in "${files[@]}"; do
        if [[ -f "$file" ]]; then
            process_file "$file"
        fi
    done
}

# Main script logic with argument parsing
main() {
    local verbose=false
    local config_file=""
    local -a input_files=()
    
    # Parse command line arguments
    while [[ $# -gt 0 ]]; do
        case $1 in
            -h|--help)
                show_usage
                exit 0
                ;;
            -v|--verbose)
                verbose=true
                shift
                ;;
            -d|--debug)
                DEBUG=true
                set -x  # Enable debug tracing
                shift
                ;;
            -c|--config)
                config_file="$2"
                shift 2
                ;;
            -*)
                echo "Unknown option: $1" >&2
                show_usage >&2
                exit 1
                ;;
            *)
                input_files+=("$1")
                shift
                ;;
        esac
    done
    
    # Load configuration if provided
    if [[ -n "$config_file" ]] && [[ -f "$config_file" ]]; then
        echo "Loading config from: $config_file"
        # Source config file safely
        source "$config_file" || {
            echo "Error loading config file" >&2
            exit 1
        }
    fi
    
    # Main processing
    echo "Starting $SCRIPT_NAME v$VERSION"
    $verbose && echo "Verbose mode enabled"
    
    # Run system checks
    run_system_checks
    
    # Process input files
    if [[ ${#input_files[@]} -gt 0 ]]; then
        echo "Processing ${#input_files[@]} file(s)..."
        for file in "${input_files[@]}"; do
            process_file "$file" || echo "Failed to process: $file"
        done
    else
        echo "No input files provided, analyzing current directory..."
        analyze_files "*.txt" *.txt 2>/dev/null
    fi
    
    # Summary
    echo
    echo "Summary:"
    echo "  Files processed: ${#FILES_PROCESSED[@]}"
    if [[ ${#CONFIG_MAP[@]} -gt 0 ]]; then
        echo "  File sizes:"
        for file in "${!CONFIG_MAP[@]}"; do
            printf "    %-20s %d bytes\n" "$file" "${CONFIG_MAP[$file]}"
        done
    fi
    
    # Cleanup and exit
    if $DEBUG; then
        set +x  # Disable debug tracing
    fi
    
    echo "Script completed successfully"
    exit 0
}

# Error handling
trap 'echo "Error on line $LINENO" >&2' ERR
trap 'echo "Script interrupted" >&2; exit 130' INT TERM

# Script entry point
if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    main "$@"
fi