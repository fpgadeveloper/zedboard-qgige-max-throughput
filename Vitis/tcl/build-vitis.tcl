# -------------------------------------------------------------------------------------
# Opsero Electronic Design Inc. Copyright 2024
# -------------------------------------------------------------------------------------

# Description
# -----------
# This Tcl script will create Vitis workspace and add a software application for the specified
# target design. If a target design is not specified, the user will be shown a list of target
# designs and asked to make a selection.

# Load functions from the workspace script
source tcl/workspace.tcl

# ----------------------------------------------------------------------------------------------
# Custom parameters
# ----------------------------------------------------------------------------------------------
# The following variables specify how the application should be created (from what 
# template if any), how things should be named and the dictionary of target designs.

# Set the Vivado directory containing the Vivado projects
set vivado_dir_rel "../Vivado"
set d_abs [file join [pwd] $vivado_dir_rel]
set vivado_dir [file normalize $d_abs]

# Set the application name
set app_name "test_app"

# Specify the postfix on the Vivado projects (if one is used)
set vivado_postfix ""

# Set the app template used to create the application
set support_app "empty_application"
set template_app "Empty Application"

# Microblaze designs: Generate combined .bit and .elf file
set mb_combine_bit_elf 1

# Possible targets (board name in lower case for the board.h file)
dict set target_dict zedboard { zedboard }

# ----------------------------------------------------------------------------------------------
# Custom modifications functions
# ----------------------------------------------------------------------------------------------
# These functions make custom changes to the platform or standard application template 
# such as modifying files or copying sources into the platform/application.
# These functions are called after creating the platform/application and before build.

proc custom_platform_mods {platform_name} {
  # No platform mods required
}

proc custom_app_mods {platform_name app_name workspace_dir} {
  # Copy common sources into the application
  copy-r "common/src" "${workspace_dir}/${app_name}/src"
}

# ----------------------------------------------------------------------------------------------
# End of custom sections
# ----------------------------------------------------------------------------------------------

# Target can be specified by creating the target variable before sourcing, or in the command line arguments
if { [info exists target] } {
  if { ![dict exists $target_dict $target] } {
    puts "Invalid target specified: $target"
    exit 1
  }
} elseif { $argc == 0 } {
  set target [select_target $target_dict]
} else {
  set target [lindex $argv 0]
  if { ![dict exists $target_dict $target] } {
    puts "Invalid target specified: $target"
    exit 1
  }
}

# At this point of the script, we are guaranteed to have a valid target
# The Vitis workspace directory name
set current_dir [pwd]
set workspace_dir [file join $current_dir "${target}_workspace"]

# Create the Vitis workspace
puts "Creating the Vitis workspace: $workspace_dir"
create_vitis_ws $workspace_dir $target $target_dict $vivado_dir $app_name $support_app $template_app


