import sys
import os
import shutil
import re

sdk_folder = "../c29_sdk"

def update_include_paths(file_path):
    """
    Update include paths in .h and .c files
    """
    # read the file contents
    with open(file_path, 'r', encoding="utf8") as file:
        filedata = file.read()

    #replace the paths
    filedata = filedata.replace('drivers/hw_include', 'security/drivers/hw_include')
    filedata = filedata.replace('security_common/drivers', 'security/drivers')
    filedata = filedata.replace('security/drivers/hw_include/f29h85x/', 'security/drivers/hw_include/')

    # write back the updated contents
    with open(file_path, 'w', encoding="utf8") as file:
        file.write(filedata)

def update_copyright_year(file_path):
    """
    Update copyright year to 2025 in files
    """
    # read the file contents
    with open(file_path, 'r', encoding="utf8") as file:
        filedata = file.read()

    # Update copyright year to 2025
    # Common copyright patterns
    # Match patterns like "Copyright (C) YYYY" or "Copyright YYYY" or "(c) YYYY"
    filedata = re.sub(r'Copyright\s+(?:\(C\)\s+)?(\d{4}(-\d{4})?)', r'Copyright (C) 2025', filedata, flags=re.IGNORECASE)
    filedata = re.sub(r'\(c\)\s+(\d{4}(-\d{4})?)', r'(c) 2025', filedata, flags=re.IGNORECASE)
    
    # write back the updated contents
    with open(file_path, 'w', encoding="utf8") as file:
        file.write(filedata)

devices = ["f29h85x"]
components = [
    os.path.join("drivers", "crypto","dthe"),
    os.path.join("drivers", "hsmclient"),
    os.path.join("drivers", "secure_ipc_notify"),
]

# List of excluded folders and files
excluded_folders = ["edma"]
excluded_files = ["cslr_hsm_ctrl.h"]

for device in devices :

    #copy tools/boot folder inside tools/boot
    src_path = os.path.join("tools", "boot")
    dest_path = os.path.join(sdk_folder, "mcu_sdk_" + device, "tools", "boot")
    if os.path.exists(dest_path):
        shutil.rmtree(dest_path)
    shutil.copytree(src_path, dest_path)

    #copy the specified components from drivers folder to sdk/source/security.
    #copy only the required soc folders. And update the include paths
    for component in components :
        src_path = component
        dest_path = os.path.join(sdk_folder, "mcu_sdk_" + device, "source", "security", component)

        if os.path.exists(dest_path):
            shutil.rmtree(dest_path)

        for root, dirs, files in os.walk(src_path):
            for file in files :
                # skip other soc files
                if "soc" in root and device not in root and os.path.basename(root) != "soc":
                    continue

                # Skip excluded folders
                skip_folder = False
                for excluded_folder in excluded_folders:
                    if excluded_folder in root:
                        print(f"Skipping excluded folder: {root}")
                        skip_folder = True
                        break
                if skip_folder:
                    continue
                
                # Skip excluded files
                if file in excluded_files:
                    print(f"Skipping excluded file: {file}")
                    continue
                
                rel_path = root.replace(src_path, "")

                # create the folder if not already present
                dest_file_path = dest_path + os.sep + rel_path
                if not os.path.exists(dest_file_path):
                    os.makedirs(dest_file_path)

                #copy the file
                src_file_path = os.path.join(root, file)
                dest_file_path = os.path.join(dest_file_path, file)
                shutil.copyfile(src_file_path, dest_file_path)

                # for c and h files, update the include paths and copyright year
                if dest_file_path.endswith((".h", ".c")):
                    update_include_paths(dest_file_path)
                    update_copyright_year(dest_file_path)
