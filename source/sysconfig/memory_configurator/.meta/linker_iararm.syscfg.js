exports = {
    defaultInstanceName: "IARARM",
	displayName: "IAR-ARM",
    config: [],
    templates:
        {
            "/memory_configurator/templates/linker.icf.xdt": {
                linker_config: "/memory_configurator/templates/linker_iararm.icf.xdt"
            }
        },
}