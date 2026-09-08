let common = system.getScript("/common");
let soc = system.getScript(`/board/ram/serialRam/soc/serialram_${common.getSocName()}`);

function getDriver(drvName) {
    return system.getScript(`/drivers/${drvName}/${drvName}`);
}

function getInstanceConfig(moduleInstance) {

    return {
        ...moduleInstance,
    };
};
let defaultDevice = soc.getDefaultDevice();

let serialRam_module = {
    displayName: defaultDevice,
    collapsed: false,
    config : getConfigurables(),
    moduleStatic: {
        modules: function(inst) {
            return [{
                name: "system_common",
                moduleName: "/system_common",
            }]
        },
    },
    validate: validate,
    moduleInstances: moduleInstances,
    getInstanceConfig,
};

function getConfigurables(){
    let config = [];

    config.push(
        {
            name: "sramName",
            displayName: defaultDevice + " Name",
            default: soc.getDefaultPsramName(),
            placeholder: "Type your psram name here",
        },
        {
            name: "sramSize",
            displayName: defaultDevice + " Size In Bytes",
            default: soc.getDefaultPsramConfig().ramSize,
            displayFormat: "dec",
        },
        {
            name: "sramPageSize",
            displayName: defaultDevice + " Page Size In Bytes",
            default: soc.getDefaultPsramConfig().pageSize,
            displayFormat: "dec",
        },
        {
            name: "sramDevcfg",
            displayName: defaultDevice + " Configuration",
            collapsed: true,
            config : [
                {
                    name: "sramDevId",
                    displayName: defaultDevice + " Device ID",
                    default: soc.getDefaultPsramConfig().deviceId,
                },
                {
                    name: "sramManfId",
                    displayName: defaultDevice + " Manufacturer ID",
                    default: soc.getDefaultPsramConfig().manufacturerId,
                },
                {
                    name: "sramCmdRd",
                    displayName: defaultDevice + " Read Command",
                    default: soc.getDefaultPsramConfig().cmdRd,
                },
                {
                    name: "sramCmdWr",
                    displayName: defaultDevice + " Write Command",
                    default: soc.getDefaultPsramConfig().cmdWr,
                },
                {
                    name: "sramCmdReset",
                    displayName: defaultDevice + " Reset Command",
                    default: soc.getDefaultPsramConfig().cmdReset,
                },
                {
                    name: "sramCmdRegRd",
                    displayName: defaultDevice + " Register Read Command",
                    default: soc.getDefaultPsramConfig().cmdRegRd,
                },
                {
                    name: "sramCmdRegWr",
                    displayName: defaultDevice + " Register Write Command",
                    default: soc.getDefaultPsramConfig().cmdRegWr,
                },
                {
                    name: "sramDummyClksRd",
                    displayName: defaultDevice + " Read Dummy Cycles",
                    default: soc.getDefaultPsramConfig().dummyClksRd,
                },
                {
                    name: "sramDummyClksWr",
                    displayName: defaultDevice + " Write Dummy Cycles",
                    default: soc.getDefaultPsramConfig().dummyClksWr,
                },
                {
                    name: "sramDummyClksCmd",
                    displayName: defaultDevice + " Command Dummy Cycles",
                    default: soc.getDefaultPsramConfig().dummyClksCmd,
                },
                {
                    name: "sramCmdExtType",
                    displayName: defaultDevice + " Command Extension Type",
                    description: "Selects how the command extension byte is formed in 8D-8D-8D / octal DDR mode",
                    default: soc.getDefaultPsramConfig().cmdExtType || "NONE",
                    options: [
                        { name: "REPEAT",  description: "Extension byte is the same as the command byte" },
                        { name: "INVERSE", description: "Extension byte is the bitwise inverse of the command byte" },
                        { name: "NONE",    description: "No command extension byte" },
                    ],
                },
            ],
        },

    );

    let defaultMrConfig = soc.getDefaultPsramConfig().mrConfig || [];
    const MR_MAX = 8;
    let mrConfigurables = [];

    for (let i = 0; i < MR_MAX; i++) {
        let defEnabled  = (i < defaultMrConfig.length);
        let defAddr     = defEnabled ? parseInt(defaultMrConfig[i].address,  16) : 0;
        let defVal      = defEnabled ? parseInt(defaultMrConfig[i].value,    16) : 0;
        let defValWidth = defEnabled ? (defaultMrConfig[i].valWidth || "1")      : "1";
        mrConfigurables.push(
            {
                name: `mr${i}`,
                displayName: defaultDevice + ` Mode Register ${i}`,
                collapsed: true,
                config: [
                    {
                        name: `mrEn${i}`,
                        displayName: "Enable",
                        default: defEnabled,
                    },
                    {
                        name: `mrAddr${i}`,
                        displayName: "Address",
                        default: defAddr,
                        displayFormat: "hex",
                    },
                    {
                        name: `mrVal${i}`,
                        displayName: "Value",
                        default: defVal,
                        displayFormat: "hex",
                    },
                    {
                        name: `mrValWidth${i}`,
                        displayName: "Value Width (bytes)",
                        description: "Number of bytes to write for this register: 1 = 8-bit, 2 = 16-bit, 4 = 32-bit",
                        default: defValWidth,
                        options: [
                            { name: "1", displayName: "1 (8-bit)"  },
                            { name: "2", displayName: "2 (16-bit)" },
                            { name: "4", displayName: "4 (32-bit)" },
                        ],
                    },
                ],
            }
        );
    }

    config.push({
        name: "mrConfig",
        displayName: defaultDevice + " Mode Register Configuration",
        collapsed: true,
        config: mrConfigurables,
    });

    return config;
}

function validate(inst, report) {
    common.validate.checkSameFieldName(inst, "device", report);
}

function moduleInstances(inst) {

    let modInstances = new Array();

    modInstances.push({
        name: "peripheralDriver",
        displayName: "OSPI Driver Configuration",
        moduleName: "/drivers/ospi/ospi",
        useArray: false,
    });

    return (modInstances);
}

exports = serialRam_module;