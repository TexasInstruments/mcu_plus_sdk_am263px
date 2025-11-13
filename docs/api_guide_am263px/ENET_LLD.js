var ENET_LLD =
[
    [ "Enet LLD Introduction", "enetlld_top.html", [
      [ "Introduction", "enetlld_top.html#enetlld_intro", null ],
      [ "Application Programming Interface", "enetlld_top.html#enetlld_api_overview", [
        [ "Control path API", "enetlld_top.html#autotoc_md1212", null ],
        [ "Data path (DMA) API", "enetlld_top.html#autotoc_md1213", null ],
        [ "Life cycle of an Enet LLD based application", "enetlld_top.html#autotoc_md1214", null ]
      ] ],
      [ "Enet Peripherals", "enetlld_top.html#enetlld_enetpers", [
        [ "CPSW Peripheral", "enetlld_top.html#enetper_cpsw", null ]
      ] ],
      [ "Document Revision History", "enetlld_top.html#enetlld_hist", null ]
    ] ],
    [ "Enet LLD IOCTL interface", "enet_ioctl_interface.html", [
      [ "Introduction", "enet_ioctl_interface.html#enet_ioctl_intro", [
        [ "Using the IOCTL interface", "enet_ioctl_interface.html#using_enet_ioctl", null ],
        [ "Synchronous and Asynchronous IOCTLs", "enet_ioctl_interface.html#enet_async_ioctl", null ]
      ] ]
    ] ],
    [ "Ethernet PHY Link Configuration", "enetphy_link_config_top.html", [
      [ "Link Configuration Guidelines", "enetphy_link_config_top.html#enetphy_link_config_guidelines", [
        [ "Manual Mode", "enetphy_link_config_top.html#enetphy_link_manual", [
          [ "Half-Duplex Mode", "enetphy_link_config_top.html#enetphy_link_manual_half_duplex", null ],
          [ "Full-Duplex Mode", "enetphy_link_config_top.html#enetphy_link_manual_full_duplex", null ]
        ] ],
        [ "Auto-Negotiation Mode", "enetphy_link_config_top.html#enetphy_link_autoneg", null ],
        [ "Strapping", "enetphy_link_config_top.html#enetphy_link_strapping", null ]
      ] ]
    ] ],
    [ "Ethernet PHY Integration Guide", "phy_integration_guide_top.html", "phy_integration_guide_top" ],
    [ "MAC2MAC support", "enet_mac2mac_top.html", [
      [ "Introduction", "enet_mac2mac_top.html#mac2mac_intro", null ],
      [ "Enable MAC2MAC through Sysconfig", "enet_mac2mac_top.html#mac2mac_syscfg_support", null ]
    ] ],
    [ "Enet Integration Guide", "enet_integration_guide_top.html", [
      [ "Introduction", "enet_integration_guide_top.html#cpsw_integration_guide_intro", null ],
      [ "Getting Familiar with Enet LLD APIs", "enet_integration_guide_top.html#GettingFamiliarWithAPIs", [
        [ "IOCTL Interface", "enet_integration_guide_top.html#IOCTL_description", null ]
      ] ],
      [ "Integrating Enet LLD into User's Application", "enet_integration_guide_top.html#enet_integration_in_app", [
        [ "Init Sequence", "enet_integration_guide_top.html#enet_init_sequence", null ],
        [ "Peripheral Open Sequence", "enet_integration_guide_top.html#enet_open_sequence", null ],
        [ "Port Open Sequence", "enet_integration_guide_top.html#enet_openport_sequence", [
          [ "MAC-PHY link", "enet_integration_guide_top.html#autotoc_md1215", null ],
          [ "MAC-to-MAC link", "enet_integration_guide_top.html#autotoc_md1216", null ]
        ] ],
        [ "Packet Send/Receive Sequence", "enet_integration_guide_top.html#enet_pktrxtx_sequence", null ],
        [ "IOCTL Sequence", "enet_integration_guide_top.html#enet_ioctl_sequence", null ],
        [ "Port Close Sequence", "enet_integration_guide_top.html#enet_closeport_sequence", null ],
        [ "Peripheral Close Sequence", "enet_integration_guide_top.html#enet_close_sequence", null ],
        [ "Deinit Sequence", "enet_integration_guide_top.html#enet_deinit_sequence", null ],
        [ "Peripheral-specific", "enet_integration_guide_top.html#enetper_specific_handling", null ]
      ] ]
    ] ],
    [ "MDIO Manual Mode Enablement", "enetmdio_manualmode.html", [
      [ "Workaround for details errata i2329-MDIO interface corruption and its impact:", "enetmdio_manualmode.html#autotoc_md1218", null ],
      [ "Limitations", "enetmdio_manualmode.html#autotoc_md1219", null ],
      [ "Not supported MDIO IOCTL APIs in MDIO Manual mode", "enetmdio_manualmode.html#autotoc_md1220", null ]
    ] ],
    [ "MDIO PHY Register Access - Clause45 Support", "enet_mdio_clause45_support.html", [
      [ "Overview", "enet_mdio_clause45_support.html#autotoc_md1221", null ],
      [ "Background", "enet_mdio_clause45_support.html#autotoc_md1222", [
        [ "Clause 22 v/s Clause 45", "enet_mdio_clause45_support.html#autotoc_md1223", null ],
        [ "MMD (Management/MDIO Manageable Device) : Required for Clause45 support", "enet_mdio_clause45_support.html#autotoc_md1224", null ]
      ] ],
      [ "Implementation Requirements", "enet_mdio_clause45_support.html#autotoc_md1225", null ],
      [ "Implementation Steps", "enet_mdio_clause45_support.html#autotoc_md1226", [
        [ "Step-1: Modify PHY Driver code with the MMD register as an input argument", "enet_mdio_clause45_support.html#autotoc_md1227", null ],
        [ "Step-2: Update EnetPhy Configuration", "enet_mdio_clause45_support.html#autotoc_md1228", null ],
        [ "Step-3: Common PHY registers access, such as PHY alive, PHY link status, etc uses Clause 22 in order to suport backward compatibility. Replace them with Clause 45 support", "enet_mdio_clause45_support.html#autotoc_md1229", null ]
      ] ],
      [ "Detailed API Reference", "enet_mdio_clause45_support.html#autotoc_md1230", [
        [ "Clause 45 Read Operation", "enet_mdio_clause45_support.html#autotoc_md1231", null ],
        [ "Clause 45 Write Operation", "enet_mdio_clause45_support.html#autotoc_md1232", null ],
        [ "Clause 45 Read-Modify-Write Operation", "enet_mdio_clause45_support.html#autotoc_md1233", null ]
      ] ],
      [ "Key Parameters Explained", "enet_mdio_clause45_support.html#autotoc_md1234", null ],
      [ "Current Limitations", "enet_mdio_clause45_support.html#autotoc_md1235", null ],
      [ "Summary", "enet_mdio_clause45_support.html#autotoc_md1236", null ],
      [ "Limitations", "enet_mdio_clause45_support.html#autotoc_md1237", null ]
    ] ],
    [ "Version Updates from earlier SDKs to Latest", "enet_mcupsdk_10_00_update.html", [
      [ "While upgrading from 11.00 SDK to 11.01 SDK", "enet_mcupsdk_10_00_update.html#autotoc_md1238", null ],
      [ "Change Set", "enet_mcupsdk_10_00_update.html#autotoc_md1239", [
        [ "Change-1 Description", "enet_mcupsdk_10_00_update.html#autotoc_md1240", null ]
      ] ],
      [ "While upgrading from 10.00 SDK", "enet_mcupsdk_10_00_update.html#autotoc_md1241", null ],
      [ "Change Set", "enet_mcupsdk_10_00_update.html#autotoc_md1242", [
        [ "Change-1 Description", "enet_mcupsdk_10_00_update.html#autotoc_md1243", null ],
        [ "Change-1 Impact", "enet_mcupsdk_10_00_update.html#autotoc_md1244", null ],
        [ "Change-1 Solution", "enet_mcupsdk_10_00_update.html#autotoc_md1245", null ],
        [ "Change-2 Description", "enet_mcupsdk_10_00_update.html#autotoc_md1246", null ],
        [ "Change-2 Impact", "enet_mcupsdk_10_00_update.html#autotoc_md1247", null ],
        [ "Change-2 Solution", "enet_mcupsdk_10_00_update.html#autotoc_md1248", null ]
      ] ],
      [ "While upgrading from 09.02 SDK or earlier", "enet_mcupsdk_10_00_update.html#autotoc_md1249", [
        [ "Change-1 Description", "enet_mcupsdk_10_00_update.html#autotoc_md1250", null ],
        [ "Change-1 Impact", "enet_mcupsdk_10_00_update.html#autotoc_md1251", null ],
        [ "Change-1 Solution", "enet_mcupsdk_10_00_update.html#autotoc_md1252", [
          [ "Option 1:", "enet_mcupsdk_10_00_update.html#autotoc_md1253", null ],
          [ "Option 2:", "enet_mcupsdk_10_00_update.html#autotoc_md1254", null ]
        ] ],
        [ "Change-2 Description", "enet_mcupsdk_10_00_update.html#autotoc_md1255", null ],
        [ "Change-2 Impact", "enet_mcupsdk_10_00_update.html#autotoc_md1256", null ],
        [ "Change-2 Solution", "enet_mcupsdk_10_00_update.html#autotoc_md1257", null ]
      ] ]
    ] ],
    [ "Ethernet Performance on AM263Px", "enetlld_performance.html", [
      [ "Introduction", "enetlld_performance.html#autotoc_md1268", null ],
      [ "Setup Details", "enetlld_performance.html#autotoc_md1269", null ],
      [ "Layer 2 Performance", "enetlld_performance.html#autotoc_md1270", [
        [ "Configuration Details", "enetlld_performance.html#autotoc_md1271", null ],
        [ "Layer 2 Latency", "enetlld_performance.html#autotoc_md1272", null ]
      ] ],
      [ "TCP/IP Performance", "enetlld_performance.html#autotoc_md1273", [
        [ "Configuration Details", "enetlld_performance.html#autotoc_md1274", [
          [ "TCP Throughput", "enetlld_performance.html#autotoc_md1275", null ],
          [ "UDP Throughput", "enetlld_performance.html#autotoc_md1276", null ]
        ] ]
      ] ],
      [ "Ether-Ring Performance", "enetlld_performance.html#autotoc_md1277", null ],
      [ "Ether-Ring Round Trip Latency with CAN(Vehicle) traffic", "enetlld_performance.html#autotoc_md1278", null ],
      [ "See Also", "enetlld_performance.html#autotoc_md1279", null ]
    ] ],
    [ "Enet EST/TAS Support", "enet_tas_top.html", [
      [ "Introduction", "enet_tas_top.html#enet_est_intro", [
        [ "IEEE 802.1Qbv EST/TAS", "enet_tas_top.html#enet_est_intro_est_tas", null ],
        [ "Guard band", "enet_tas_top.html#enet_est_intro_guard_band", null ]
      ] ],
      [ "Enet LLD API", "enet_tas_top.html#enet_est_api", null ],
      [ "CPSW Support", "enet_tas_top.html#enet_est_cpsw", [
        [ "CPSW EST Driver Implementation", "enet_tas_top.html#enet_est_cpsw_driver", null ],
        [ "Programing Guidelines and Limitations", "enet_tas_top.html#enet_est_cpsw_guidelines", [
          [ "Administrative base time", "enet_tas_top.html#enet_est_cpsw_guidelines_admin_basetime", null ],
          [ "Gate control list", "enet_tas_top.html#enet_est_cpsw_guidelines_gate_control_list", null ],
          [ "Guard band", "enet_tas_top.html#enet_est_cpsw_guidelines_guard_band", null ],
          [ "Link-down event", "enet_tas_top.html#enet_est_cpsw_guidelines_link_down", null ],
          [ "Limitations", "enet_tas_top.html#enet_est_cpsw_limitations", null ]
        ] ],
        [ "Debugging and Troubleshooting", "enet_tas_top.html#enet_est_cpsw_debugging", [
          [ "EST Timestamping", "enet_tas_top.html#enet_est_cpsw_timestamping", null ],
          [ "CCS Debug GEL Files", "enet_tas_top.html#enet_est_cpsw_gels", null ]
        ] ]
      ] ]
    ] ],
    [ "Enet Migration Guide", "enet_migration_guide_top.html", [
      [ "Introduction", "enet_migration_guide_top.html#enet_migration_guide_intro", null ],
      [ "Need for sysconfig", "enet_migration_guide_top.html#NeedForSysconfig", null ],
      [ "Enet Driver Initialization Sequence Change", "enet_migration_guide_top.html#EnetInitSeqChange", null ],
      [ "Enet DMA channel open changes", "enet_migration_guide_top.html#EnetDmaChOpenChange", null ],
      [ "Runtime Control API (Enet_ioctl) change", "enet_migration_guide_top.html#EnetIoctlChange", null ],
      [ "Packet Transmit/Receive API related changes", "enet_migration_guide_top.html#PacketTxRxChange", null ],
      [ "Custom Board Support", "enet_migration_guide_top.html#CustomBoardSupport", null ],
      [ "External PHY management", "enet_migration_guide_top.html#ExternalPhyManagement", null ],
      [ "Tuning memory usage of enet driver for non-lwip apps", "enet_migration_guide_top.html#MemoryTuningNonLwip", null ],
      [ "Tuning memory usage of enet driver for lwip apps", "enet_migration_guide_top.html#MemoryTuningLwip", null ],
      [ "Compatibilty Breaks During MCU+ SDK Version Update", "enet_migration_guide_top.html#autotoc_md1265", [
        [ "Updating to version 10.00", "enet_migration_guide_top.html#autotoc_md1266", null ]
      ] ]
    ] ],
    [ "Ethernet Add-on Boards", "ETHERNET_ADDON_BOARDS_TOP.html", [
      [ "Introduction", "ETHERNET_ADDON_BOARDS_TOP.html#ethernet_addon_boards_intro", [
        [ "Important Usage Guidelines", "ETHERNET_ADDON_BOARDS_TOP.html#autotoc_md1267", null ]
      ] ]
    ] ],
    [ "Ethernet Packet Pool Allocation Guidelines", "PACKETPOOL_CONFIG_TOP.html", null ],
    [ "Ether-Ring Overview", "ETHERRING_OVERVIEW.html", [
      [ "Software Architecture", "ETHERRING_OVERVIEW.html#autotoc_md1280", null ],
      [ "CAN to Ethernet Traffic Simulation", "ETHERRING_OVERVIEW.html#autotoc_md1281", null ],
      [ "Packet Duplication on Transmission", "ETHERRING_OVERVIEW.html#autotoc_md1282", null ],
      [ "Software Assistance on Transmission side", "ETHERRING_OVERVIEW.html#autotoc_md1283", null ],
      [ "Software Assistance on Reception side", "ETHERRING_OVERVIEW.html#autotoc_md1284", null ],
      [ "Duplicate packet Rejection on Reception side(Duplicate Packet Rejection)", "ETHERRING_OVERVIEW.html#autotoc_md1285", null ]
    ] ]
];