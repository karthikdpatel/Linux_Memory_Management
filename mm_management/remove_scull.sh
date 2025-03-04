#!/bin/bash

MODULE_NAME="mm_simulatorko"
echo "Starting the removal process for the $MODULE_NAME module."

# Function to handle errors
error_handler() {
    echo "An error occurred. Exiting the script."
    exit 1
}

# Function to remove the module
remove_module() {
    echo "Removing module $MODULE_NAME..."
    sudo rmmod $MODULE_NAME
    echo "Module $MODULE_NAME removed successfully."
}

remove_module
echo "Removal process completed successfully."

