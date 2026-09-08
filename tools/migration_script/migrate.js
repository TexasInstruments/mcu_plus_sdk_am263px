const fs = require("fs");
const util = require("util");
const path = require("path");
const yargs = require("yargs");

//calculate the mcu_plus_sdk path using current directory
let mcusdkPath = __dirname.split(path.sep).slice(0,-2).join(path.sep);

//read imports.mak file to get sysconfig version
const importsFile = fs.readFileSync(path.join(mcusdkPath, 'imports.mak'), "utf-8");
let syscfgRegex = /sysconfig.+/gm;
let sysconfigVersion = "sysconfig_1.19.0";
const match = importsFile.match(syscfgRegex);
if (null != match) {
    sysconfigVersion = match[0];
}

//calculating the sysconfig path based on the environment
let OS = process.platform;
let syscfgPath = "";
if (OS == "win32") {
    syscfgPath = `C:\\ti\\${sysconfigVersion}\\tests\\sysConfig`;
}
if (OS == "linux") {
    syscfgPath = process.env["HOME"] + `/ti/${sysconfigVersion}/tests/sysConfig`;
}

//load supported devices from deviceData
let supportedDevices = [];
try {
    let deviceJsonPath = path.join(syscfgPath.split(path.sep).slice(0, -2).join(path.sep), 'dist', 'deviceData', 'devices.json');
    let deviceJsonData = fs.readFileSync(deviceJsonPath);
    let devices = JSON.parse(deviceJsonData).devices;
    supportedDevices = devices.map(d => d.name).sort();
} catch (e) {
    // if we can't load devices, continue anyway - they'll be validated later
}

//function to format device list for help text
function formatDeviceList() {
    if (supportedDevices.length === 0) {
        return "Unable to load device list. Ensure SysConfig is installed.";
    }

    // Filter to only show supported devices
    const relevantFamilies = ['AM261x', 'AM263Px', 'AM273x'];
    let filtered = supportedDevices.filter(device =>
        relevantFamilies.some(family => device.startsWith(family))
    );

    let grouped = {};
    filtered.forEach(device => {
        let family = device.split('_')[0]; 
        if (!grouped[family]) {
            grouped[family] = [];
        }
        grouped[family].push(device);
    });

    let output = "\nCASE SENSITIVE: Device names are case-sensitive. Use exact case as shown below.\n";
    output += "\nSupported Devices:\n";

    ['AM261x', 'AM263Px', 'AM273x'].forEach(family => {
        if (grouped[family]) {
            output += `\n  ${family}:\n`;
            grouped[family].forEach(device => {
                output += `    • ${device}\n`;
            });
        }
    });

    return output;
}

//arguments
const argv = yargs
    .option('product', {
        alias: 'p',
        describe: 'Path to product.json file',
        type: 'string',
        demandOption: true,
    })
    .option('sourceDevice', {
        alias: 'srcdev',
        describe: 'Source device',
        type: 'string',
        demandOption: true,
    })
    .option('destinationDevice', {
        alias: 'destdev',
        describe: 'Destination device',
        type: 'string',
        demandOption: true,
    })
    .option('sourcePackage', {
        alias: 'srcpkg',
        describe: 'Source package',
        type: 'string',
        default: null,
    })
    .option('destinationPackage', {
        alias: 'destpkg',
        describe: 'Destination package',
        type: 'string',
        default: null,
    })
    .option('sourcePart', {
        alias: 'srcpart',
        describe: 'Source part',
        type: 'string',
        default: null,
    })
    .option('destinationPart', {
        alias: 'destpart',
        describe: 'Destination part',
        type: 'string',
        default: null,
    })
    .option('migratePath', {
        alias: 'sdkpath',
        describe: 'Path of folder to be migrated. Ideally the mcu_sdk path, but can be path to individual examples also.',
        type: 'string',
        demandOption: true,
    })
    .help('h')
    .alias('h', 'help')
    .epilogue(formatDeviceList())
    .parse(process.argv.slice(2), { allowEquals: true });

//command line arguments
let {
    product,
    sourceDevice,
    destinationDevice,
    sourcePackage,
    destinationPackage,
    sourcePart,
    destinationPart,
    migratePath
} = argv;

//convert arguments to sysconfig env supported format eg: AM263Px, AM261x_ZFG, AM263P4

if (sourceDevice) {
    if (sourceDevice.includes('_')) {
        
        let match = sourceDevice.match(/^([A-Za-z0-9]+)_([A-Z]+)(_\d+)?$/i);
        if (match) {
            // Format: [base]_[package](_[variant])?
            let base = match[1].slice(0, -1).toUpperCase() + match[1].slice(-1).toLowerCase();
            let pkg = match[2].toUpperCase();
            let variant = match[3] ? match[3].toUpperCase() : '';
            sourceDevice = base + '_' + pkg + variant;
        } else {
            sourceDevice = sourceDevice.toUpperCase();
        }
    } else {
        // Legacy format without package: AM263Px
        sourceDevice = sourceDevice.slice(0, -1).toUpperCase() + sourceDevice.slice(-1).toLowerCase();
    }
}
if (destinationDevice) {

    if (destinationDevice.includes('_')) {
        
        let match = destinationDevice.match(/^([A-Za-z0-9]+)_([A-Z]+)(_\d+)?$/i);
        if (match) {
            // Format: [base]_[package](_[variant])?
            let base = match[1].slice(0, -1).toUpperCase() + match[1].slice(-1).toLowerCase();
            let pkg = match[2].toUpperCase();
            let variant = match[3] ? match[3].toUpperCase() : '';
            destinationDevice = base + '_' + pkg + variant;
        } else {
            destinationDevice = destinationDevice.toUpperCase();
        }
    } else {
        // Legacy format without package: AM263Px
        destinationDevice = destinationDevice.slice(0, -1).toUpperCase() + destinationDevice.slice(-1).toLowerCase();
    }
}
if (sourcePackage)
    sourcePackage = sourcePackage.toUpperCase();
if (destinationPackage)
    destinationPackage = destinationPackage.toUpperCase();
if (sourcePart)
    sourcePart = sourcePart.toUpperCase();
if (destinationPart)
    destinationPart = destinationPart.toUpperCase();

//load sysconfig module
const sysConfig = require(syscfgPath);

// make list of all the examples paths to be excluded
let excludePaths = read_exclude_list();

//function to validate the inputs
function validate_inputs() {

    console.log("Validating inputs...");

    //Parse sysconfig deviceData
    let deviceJsonPath = path.join(syscfgPath.split(path.sep).slice(0, -2).join(path.sep), 'dist', 'deviceData', 'devices.json')
    let deviceJsonData = fs.readFileSync(deviceJsonPath)
    let devices = JSON.parse(deviceJsonData).devices;

    // Find the source and destination devices in the JSON data
    let srcDevice = devices.find(device => device.name === sourceDevice);
    let destDevice = devices.find(device => device.name === destinationDevice);

    if (!srcDevice && sourceDevice.includes('_')) {
        console.log(`Note: Device name '${sourceDevice}' includes package suffix. For AM261x devices, use format: AM261x_ZFG or similar.`);
    }

    if (!destDevice && destinationDevice.includes('_')) {
        console.log(`Note: Device name '${destinationDevice}' includes package suffix. For AM261x devices, use format: AM261x_ZNC or similar.`);
    }

    // Validate source device
    if (!srcDevice) {
        console.log(`Source device '${sourceDevice}' not found in deviceData.\nAvailable devices include: AM261x_ZFG, AM261x_ZNC, AM263Px, etc.\nMigration aborted !!`);
        return false;
    }

    // Validate destination device
    if (!destDevice) {
        console.log(`Destination device '${destinationDevice}' not found in deviceData.\nAvailable devices include: AM261x_ZFG, AM261x_ZNC, AM263Px, etc.\nMigration aborted !!`);
        return false;
    }

    // Validate source,destination package
    // Skip package validation if device name already includes the package (e.g., AM261x_ZFG)
    if (sourcePackage && !sourceDevice.includes('_')) {
        if (!srcDevice.package.find(pkg => pkg.name === sourcePackage)) {
            console.log(`Package '${sourcePackage}' not found for source device '${sourceDevice}' in deviceData.\nMigration aborted !!`);
            return false;
        }
    }
    if (destinationPackage && !destinationDevice.includes('_')) {
        if (!destDevice.package.find(pkg => pkg.name === destinationPackage)) {
            console.log(`Package '${destinationPackage}' not found for destination device '${destinationDevice}' in deviceData.\nMigration aborted !!`);
            return false;
        }
    }

    // Validate source,destination part
    if (sourcePart && !srcDevice.part.find(part => part.name === sourcePart)) {
        console.log(`Part '${sourcePart}' not found for source device '${sourceDevice}' in deviceData.\nMigration aborted !!`);
        return false;
    }
    if (destinationPart && !destDevice.part.find(part => part.name === destinationPart)) {
        console.log(`Part '${destinationPart}' not found for destination device '${destinationDevice}' in deviceData.\nMigration aborted !!`);
        return false;
    }

    console.log("Inputs validation successful.");
    return true;
}

//function to list all the paths to be excluded from migration -- input from exclude_list.txt
function read_exclude_list() {
    let baseDeviceName = sourceDevice.includes('_') ? sourceDevice.split('_')[0] : sourceDevice;
    let exclude_list_filename = `exclude_list_${baseDeviceName.toLowerCase()}.js`
    const content = require(path.join(mcusdkPath, 'tools', 'migration_script', 'soc', `${exclude_list_filename}`))
    return content.getExcludeList();
}

//function to handle OSPI pin assignments for migration
function prepareOspiPinAssignmentsForMigration(syscfgContent, filePath) {

    if (sourceDevice !== 'AM263Px') {
        return syscfgContent;
    }
    const ospiRegex = /(?:CONFIG_OSPI\d*|\.OSPI\.|\bospi\b|drivers_ospi|(?:peripheralDriver\.)+OSPI)/i;
    if ((syscfgContent.includes('const flash') && syscfgContent.includes('peripheralDriver.OSPI')) || 
        ospiRegex.test(syscfgContent))
    {
        const lines = syscfgContent.split('\n');
        const ospiAssignmentLines = [];
        const ospiAssignments = [];

        for (let i = 0; i < lines.length; i++) {
            // Updated regex to capture pins with underscores like RESET_OUT0
            if (lines[i].match(/(?:peripheralDriver\.OSPI|ospi\d+\.OSPI)\.[A-Za-z0-9_]+\.\$assign\s*=/)) {
                ospiAssignmentLines.push(i);
                // Extract the assignment, convert to suggestSolution, and store
                const line = lines[i].trim();
                const cleanedAssignment = line.replace(/\s*;?\s*$/, ''); // Remove trailing semicolon and whitespace
                ospiAssignments.push(cleanedAssignment.replace('$assign', '$suggestSolution'));
            }
        }
        
        if (ospiAssignments.length === 0) {
            return syscfgContent;
        }

        for (let i = ospiAssignmentLines.length - 1; i >= 0; i--) {
            const lineIndex = ospiAssignmentLines[i];
            lines.splice(lineIndex, 1);
        }

        let newContent = lines.join('\n');

        const pinmuxSectionRegex = /\/\*\*[\s\S]*?Pinmux solution[\s\S]*?\*\/\s*\n/;
        const pinmuxMatch = pinmuxSectionRegex.exec(newContent);

        if (pinmuxMatch) {

            const formattedAssignments = ospiAssignments.map(a => a + ';').join('\n');

            const beforeSection = newContent.substring(0, pinmuxMatch.index + pinmuxMatch[0].length);
            const afterSection = newContent.substring(pinmuxMatch.index + pinmuxMatch[0].length);

            newContent = beforeSection + formattedAssignments + '\n' + afterSection;
        } else {

            const formattedAssignments = ospiAssignments.map(a => a + ';').join('\n');

            newContent += '\n\n/**\n * Pinmux solution for unlocked pins/peripherals. This ensures that minor changes to the automatic solver in a future\n * version of the tool will not impact the pinmux you originally saw.\n */\n' + formattedAssignments + '\n';
        }
        
        fs.writeFileSync(filePath, newContent);
        
        return newContent;
    }
    
    return syscfgContent;
}

//function to recursively travel all folders inside given path and perform migration
const get_all_files = async function (dirPath, arrayOfFiles) {

    let files = fs.readdirSync(dirPath)
    arrayOfFiles = arrayOfFiles || []

    for (let file of files) {

        let filePath = path.join(dirPath, file)

        if (fs.statSync(filePath).isDirectory()) {
            if(excludePaths.length === 0 || !excludePaths.some(excpath => filePath.includes(excpath))) {
                arrayOfFiles = await get_all_files(filePath, arrayOfFiles);
            }
        }
        else {
            // Extract base device name for path matching (e.g., AM261x_ZFG -> am261x)
            let baseDeviceForPathMatch = sourceDevice.includes('_') ? sourceDevice.split('_')[0] : sourceDevice;
            if (dirPath.includes(baseDeviceForPathMatch.toLowerCase())) {

                if (path.extname(file) == ".syscfg") {

                    //read example.syscfg
                    let data = fs.readFileSync(filePath, "utf-8");
                    //check if the file needs to be migrated using a regex match of given sourcePackage/sourcePart
                    let pkgRegex = null, partRegex = null
                    if (null != sourcePackage) {
                        pkgRegex = new RegExp(sourcePackage, "gmi")
                    }
                    if (null != sourcePart) {
                        partRegex = new RegExp(sourcePart, "gmi")
                    }

                    if ((null != data.match(pkgRegex)) || (null != data.match(partRegex))) {
                        try {
                            data = prepareOspiPinAssignmentsForMigration(data, filePath);
                            let { internals } = await sysConfig.asyncCreateEnv([
                                "--product",
                                product,
                                "--device",
                                sourceDevice,
                                "--package",
                                sourcePackage,
                                "--script",
                                filePath,
                            ]);
                            
                            internals = (
                                await internals.asyncMigrate({ device: destinationDevice, package: destinationPackage, part: destinationPart }, false, undefined, true)
                            ).internals;

                            const newScript = await internals.asyncSerialize();
                            fs.writeFileSync(filePath, newScript);

                            migrateDone.push(filePath.replace(mcusdkPath, ""));
                        }
                        catch (e) {
                            migrateFail.push(filePath.replace(mcusdkPath, ""));
                            migrateFail.push("\n" + util.format(e));
                        }
                    }
                }

                //update the respective makefile and example.projectspec also only if the example is migrated
                let makefileRegex = /makefile$/g
                let projectspecRegex = /example.projectspec$/g
                let sourcePackageRegex = new RegExp(sourcePackage, "g")
                let sourcePartRegex = new RegExp(sourcePart, "g")
                const pathsep = OS === "win32" ? '\\\\' : '/'
                // Use base device name for path matching (e.g., am261x instead of AM261x_ZFG)
                let checkRegex = new RegExp(`${pathsep}.+` + baseDeviceForPathMatch, "gmi")

                if (migrateDone.some(ele => checkRegex.test(ele))) {
                    if ((null != filePath.match(projectspecRegex)) || (null != filePath.match(makefileRegex))) {
                        let data = fs.readFileSync(filePath, "utf-8");
                        if (null != data.match(sourcePackageRegex)) {
                            
                            let sourceDeviceRegex = new RegExp(sourceDevice.replace(/[.*+?^${}()|[\]\\]/g, '\\$&'), "g");
                            data = data.replace(sourceDeviceRegex, destinationDevice);
                            
                            data = data.replace(sourcePackageRegex, destinationPackage);
                            data = data.replace(sourcePartRegex, destinationPart);
                            fs.writeFileSync(filePath, data);
                        }
                    }
                }

            }
        }
    }
    return arrayOfFiles
};

//create log file
const logger = fs.createWriteStream(path.join(__dirname,'log_file.txt'));
let migrateDone = [];
let migrateFail = [];

//entry point
(async () => {
    //exit if the inputs are not correct
    if (!validate_inputs()) return;

    console.log("Migration started...")
    await get_all_files(migratePath);
    console.log("Migration completed.")

    //write content to the log file
    logger.write("FILE(S) MIGRATED: \n");
    for (let i = 0; i < migrateDone.length; i++) {
        logger.write(migrateDone[i] + "\n");
    }
    logger.write("\nFILE(S) NOT MIGRATED: \n");
    for (let i = 0; i < migrateFail.length; i++) {
        logger.write(migrateFail[i] + "\n");
    }
})();