
let common = system.getScript("/common");
let soc = system.getScript(`/drivers/mcu_pbist/soc/mcu_pbist_${common.getSocName()}`);

function getInstanceConfig(moduleInstance) {
    let pbistConfig = getPbistConfig(moduleInstance);

    return {
        ...moduleInstance,
        selectedInstances: pbistConfig.selectedInstances,
        coreList: pbistConfig.coreList,
        auxDevices: pbistConfig.auxDevices,
        pscAddrs: pbistConfig.pscAddrs,
        pscPds: pbistConfig.pscPds,
        lpscIds: pbistConfig.lpscIds,
        auxPscs: pbistConfig.auxPscs,
    };
};

function getPbistConfig(instance)
{
    let pbistConfig = soc.getSdlMcuPbistInstanceConfig();
    var selectedInstances = [];
    var auxDevices = [];
    var pscAddrs = [];
    var pscPds = [];
    var lpscIds = [];
    var auxPscs = [];

    for (var i = 0; i < pbistConfig.length; i++)
    {
        if (instance[pbistConfig[i].instance])
        {
            selectedInstances.push(pbistConfig[i].instance);
            auxDevices.push(pbistConfig[i].auxDevices);

            for (var j = 0; j < pbistConfig[i].pscConfig.length; j++)
            {
                pscAddrs.push(pbistConfig[i].pscConfig[j].pscAddress);
                pscPds.push(pbistConfig[i].pscConfig[j].pscPd);
                lpscIds.push(pbistConfig[i].pscConfig[j].lpscId);
                auxPscs.push(pbistConfig[i].pscConfig[j].auxDevice);
            }
        }
    }

    return {
        selectedInstances: selectedInstances,
        coreList: selectedInstances.map(item => item.split('_').slice(3).join('_')),
        auxDevices: auxDevices,
        pscAddrs: pscAddrs,
        pscPds: pscPds,
        lpscIds: lpscIds,
        auxPscs: auxPscs,
    };
}

function getConfigurables() {
    let pbistConfig = soc.getSdlMcuPbistInstanceConfig();
    var pbistInstances = [];

    for (var i = 0; i < pbistConfig.length; i++)
    {
        pbistInstances.push(pbistConfig[i].instance);
    }

    let pbistChecks = pbistInstances.map(checkerInstance => ({
        name: `${checkerInstance}`,
        displayName: `${checkerInstance}`,
        description: `${checkerInstance}`,
        default: (checkerInstance === "SDL_PBIST_INST_MCU"),
    }));

    let configurables = [
        {
            name: "enableMcuPbist",
            displayName: "Start PBIST On System Init",
            description: "Start selected PBIST instances on System_init()",
            default: true,
        },
        {
            name: "pbistInsts",
            displayName: "Select PBIST Instances",
            description: "PBIST tests of all selected instances will run during System_init()",
            collapsed: false,
            config: pbistChecks,
        },
    ];

    return configurables;
}

function validate(instance, report) {
    let pbistConfig = getPbistConfig(instance);

    if (instance.enableMcuPbist && pbistConfig.selectedInstances.length === 0)
    {
        report.logError("When PBIST is enabled, select at least one PBIST instance", instance, "pbistInsts");
    }
}

let bist_module = {
    displayName: "MCU_PBIST",

    templates: {
        "/drivers/system/system_config.c.xdt": {
            driver_config: "/drivers/mcu_pbist/templates/mcu_pbist_config.c.xdt",
            driver_init: "/drivers/mcu_pbist/templates/mcu_pbist_init.c.xdt",
        },
        "/drivers/system/system_config.h.xdt": {
            driver_config: "/drivers/mcu_pbist/templates/mcu_pbist.h.xdt",
        },
    },
    maxInstances: 1,
    defaultInstanceName: "CONFIG_MCU_PBIST",
    config: getConfigurables(),
    validate: validate,
    moduleStatic: {
        modules: function(instance) {
            return [{
                name: "system_common",
                moduleName: "/system_common",
            }]
        },
    },
    getInstanceConfig,
};

exports = bist_module;
